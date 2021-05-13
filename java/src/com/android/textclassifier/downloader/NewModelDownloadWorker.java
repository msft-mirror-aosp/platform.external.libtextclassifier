/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.textclassifier.downloader;

import android.content.Context;
import androidx.work.Data;
import androidx.work.ListenableWorker;
import androidx.work.WorkerParameters;
import com.android.textclassifier.common.ModelFileManager;
import com.android.textclassifier.common.ModelType;
import com.android.textclassifier.common.ModelType.ModelTypeDef;
import com.android.textclassifier.common.TextClassifierServiceExecutors;
import com.android.textclassifier.common.TextClassifierSettings;
import com.android.textclassifier.common.base.TcLog;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.util.concurrent.FluentFuture;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import java.io.File;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.concurrent.ExecutorService;

// TODO(licha): Rename this to ModelDownloadWorker.
// TODO(licha): Consider whether we should let the worker handle all locales/model types.
/** The WorkManager worker to download models for TextClassifierService. */
public final class NewModelDownloadWorker extends ListenableWorker {
  private static final String TAG = "NewModelDownloadWorker";
  private static final int MAX_DOWNLOAD_ATTEMPTS_DEFAULT = 5;

  static final String DATA_MODEL_TYPE_KEY = "NewDownloadWorker_modelType";
  static final String DATA_LOCALE_TAG_KEY = "NewDownloadWorker_localeTag";
  static final String DATA_MANIFEST_URL_KEY = "NewDownloadWorker_manifestUrl";
  static final String DATA_MAX_DOWNLOAD_ATTEMPTS_KEY = "NewDownloadWorker_maxDownloadAttempts";

  @ModelTypeDef private final String modelType;
  private final String manifestUrl;
  private final int maxDownloadAttempts;

  private final ExecutorService executorService;
  private final ModelDownloader downloader;
  private final File modelDownloaderDir;
  private final Runnable postDownloadCleanUpRunnable;

  public NewModelDownloadWorker(Context context, WorkerParameters workerParams) {
    super(context, workerParams);

    this.modelType = Preconditions.checkNotNull(getInputData().getString(DATA_MODEL_TYPE_KEY));
    this.manifestUrl = Preconditions.checkNotNull(getInputData().getString(DATA_MANIFEST_URL_KEY));
    this.maxDownloadAttempts =
        getInputData().getInt(DATA_MAX_DOWNLOAD_ATTEMPTS_KEY, MAX_DOWNLOAD_ATTEMPTS_DEFAULT);

    this.executorService = TextClassifierServiceExecutors.getDownloaderExecutor();
    this.downloader = new ModelDownloaderImpl(context, executorService);
    ModelFileManager modelFileManager = new ModelFileManager(context, new TextClassifierSettings());
    this.modelDownloaderDir = modelFileManager.getModelDownloaderDir();
    this.postDownloadCleanUpRunnable = modelFileManager::deleteUnusedModelFiles;
  }

  @VisibleForTesting
  NewModelDownloadWorker(
      Context context,
      WorkerParameters workerParams,
      ExecutorService executorService,
      ModelDownloader modelDownloader,
      File modelDownloaderDir,
      Runnable postDownloadCleanUpRunnable) {
    super(context, workerParams);

    this.modelType = Preconditions.checkNotNull(getInputData().getString(DATA_MODEL_TYPE_KEY));
    this.manifestUrl = Preconditions.checkNotNull(getInputData().getString(DATA_MANIFEST_URL_KEY));
    this.maxDownloadAttempts =
        getInputData().getInt(DATA_MAX_DOWNLOAD_ATTEMPTS_KEY, MAX_DOWNLOAD_ATTEMPTS_DEFAULT);

    this.executorService = executorService;
    this.downloader = modelDownloader;
    this.modelDownloaderDir = modelDownloaderDir;
    this.postDownloadCleanUpRunnable = postDownloadCleanUpRunnable;
  }

  @Override
  public final ListenableFuture<ListenableWorker.Result> startWork() {
    if (getRunAttemptCount() >= maxDownloadAttempts) {
      TcLog.d(TAG, "Max attempt reached. Abort download task.");
      TextClassifierDownloadLogger.downloadFailedAndAbort(
          modelType,
          manifestUrl,
          // TODO(licha): Add a new failure reason for this
          ModelDownloadException.UNKNOWN_FAILURE_REASON,
          getRunAttemptCount());
      return Futures.immediateFuture(ListenableWorker.Result.failure());
    }
    FluentFuture<ListenableWorker.Result> resultFuture =
        FluentFuture.from(downloader.downloadManifest(manifestUrl))
            .transformAsync(
                manifest -> {
                  // TODO(licha): put this in a function to improve the readability
                  ModelManifest.Model modelInfo = manifest.getModels(0);
                  File targetModelFile =
                      new File(
                          modelDownloaderDir,
                          formatFileNameByModelTypeAndUrl(modelType, modelInfo.getUrl()));
                  if (targetModelFile.exists()) {
                    TcLog.d(
                        TAG,
                        "Target model file already exists. Skip download and reuse it: "
                            + targetModelFile.getAbsolutePath());
                    TextClassifierDownloadLogger.downloadSucceeded(
                        modelType, manifestUrl, getRunAttemptCount());
                    return Futures.immediateFuture(ListenableWorker.Result.success());
                  } else {
                    return Futures.transform(
                        downloadAndMoveModel(targetModelFile, modelInfo),
                        unused -> {
                          TextClassifierDownloadLogger.downloadSucceeded(
                              modelType, manifestUrl, getRunAttemptCount());
                          return ListenableWorker.Result.success();
                        },
                        executorService);
                  }
                },
                executorService)
            .catching(
                Throwable.class,
                e -> {
                  TcLog.e(TAG, "Download attempt failed.", e);
                  int errorCode =
                      (e instanceof ModelDownloadException)
                          ? ((ModelDownloadException) e).getErrorCode()
                          : ModelDownloadException.UNKNOWN_FAILURE_REASON;
                  // Retry until reach max allowed attempts (attempt starts from 0)
                  // The backoff time between two tries will grow exponentially (i.e. 30s, 1min,
                  // 2min, 4min). This is configurable when building the request.
                  TextClassifierDownloadLogger.downloadFailedAndRetry(
                      modelType, manifestUrl, errorCode, getRunAttemptCount());
                  return ListenableWorker.Result.retry();
                },
                executorService);
    resultFuture.addListener(postDownloadCleanUpRunnable, executorService);
    return resultFuture;
  }

  private ListenableFuture<Void> downloadAndMoveModel(
      File targetModelFile, ModelManifest.Model modelInfo) {
    return Futures.transform(
        downloader.downloadModel(modelInfo),
        pendingModelFile -> {
          try {
            if (!modelDownloaderDir.exists()) {
              modelDownloaderDir.mkdirs();
            }
            Files.move(
                pendingModelFile.toPath(),
                targetModelFile.toPath(),
                StandardCopyOption.ATOMIC_MOVE,
                StandardCopyOption.REPLACE_EXISTING);
            TcLog.d(
                TAG, "Model file downloaded successfully: " + targetModelFile.getAbsolutePath());
            return null;
          } catch (Exception e) {
            pendingModelFile.delete();
            throw new ModelDownloadException(ModelDownloadException.FAILED_TO_MOVE_MODEL, e);
          }
        },
        executorService);
  }

  /**
   * This method will be called when we our work gets interrupted by the system. Result future
   * should have already been cancelled in that case. Unless it's because the REPLACE policy of
   * WorkManager unique queue, the interrupted work will be rescheduled later.
   */
  @Override
  public final void onStopped() {
    TcLog.d(TAG, String.format("Stop download: %s, attempt:%d", manifestUrl, getRunAttemptCount()));
    TextClassifierDownloadLogger.downloadFailedAndRetry(
        modelType, manifestUrl, ModelDownloadException.WORKER_STOPPED, getRunAttemptCount());
  }

  static final Data createInputData(
      @ModelTypeDef String modelType,
      String localeTag,
      String manifestUrl,
      int maxDownloadAttempts) {
    return new Data.Builder()
        .putString(DATA_MODEL_TYPE_KEY, modelType)
        .putString(DATA_LOCALE_TAG_KEY, localeTag)
        .putString(DATA_MANIFEST_URL_KEY, manifestUrl)
        .putInt(DATA_MAX_DOWNLOAD_ATTEMPTS_KEY, maxDownloadAttempts)
        .build();
  }

  /**
   * Returns the absolute path to download a model.
   *
   * <p>Each file's name is uniquely formatted based on its unique remote manifest URL.
   *
   * @param modelType the type of the model image to download
   * @param url the unique remote url
   */
  static String formatFileNameByModelTypeAndUrl(
      @ModelType.ModelTypeDef String modelType, String url) {
    // TODO(licha): Consider preserving the folder hierarchy of the URL
    String fileMidName = url.replaceAll("[^A-Za-z0-9]", "_");
    if (fileMidName.startsWith("https___")) {
      fileMidName = fileMidName.substring("https___".length());
    }
    return String.format("%s.%s.model", modelType, fileMidName);
  }
}
