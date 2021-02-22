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

package com.android.textclassifier;

import android.content.Context;
import androidx.work.Data;
import androidx.work.ListenableWorker;
import androidx.work.WorkerParameters;
import com.android.textclassifier.common.base.TcLog;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.util.concurrent.FluentFuture;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import java.io.File;
import java.net.URI;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Abstract worker to download specified manifest/model. Subclasses only need to implement the logic
 * to handle the downloaded file. Scheduled/executed by WorkManager.
 */
abstract class AbstractDownloadWorker extends ListenableWorker {
  private static final String TAG = "DownloadWorker";

  @VisibleForTesting static final String DATA_URL_KEY = "DownloadWorker_url";

  @VisibleForTesting
  static final String DATA_DESTINATION_PATH_KEY = "DownloadWorker_destinationPath";

  @VisibleForTesting
  static final String DATA_REUSE_EXISTING_FILE_KEY = "DownloadWorker_reuseExistingFile";

  @VisibleForTesting
  static final String DATA_MAX_DOWNLOAD_ATTEMPTS_KEY = "DownloadWorker_maxAttempts";

  private static final boolean DATA_REUSE_EXISTING_FILE_DEFAULT = false;
  private static final int DATA_MAX_DOWNLOAD_ATTEMPTS_DEFAULT = 5;

  private final String url;
  private final String destinationPath;
  private final boolean reuseExistingFile;
  private final int maxDownloadAttempts;

  // TODO(licha): Maybe create some static executors and share them across tcs
  private final ExecutorService bgExecutorService;
  private final ModelDownloader downloader;

  AbstractDownloadWorker(Context context, WorkerParameters workerParams) {
    this(context, workerParams, Executors.newSingleThreadExecutor());
  }

  private AbstractDownloadWorker(
      Context context, WorkerParameters workerParams, ExecutorService bgExecutorService) {
    this(
        context,
        workerParams,
        bgExecutorService,
        new ModelDownloaderImpl(context, bgExecutorService));
  }

  @VisibleForTesting
  AbstractDownloadWorker(
      Context context,
      WorkerParameters workerParams,
      ExecutorService bgExecutorService,
      ModelDownloader downloader) {
    super(context, workerParams);

    this.url = Preconditions.checkNotNull(getInputData().getString(DATA_URL_KEY));
    this.destinationPath =
        Preconditions.checkNotNull(getInputData().getString(DATA_DESTINATION_PATH_KEY));
    this.reuseExistingFile =
        getInputData().getBoolean(DATA_REUSE_EXISTING_FILE_KEY, DATA_REUSE_EXISTING_FILE_DEFAULT);
    this.maxDownloadAttempts =
        getInputData().getInt(DATA_MAX_DOWNLOAD_ATTEMPTS_KEY, DATA_MAX_DOWNLOAD_ATTEMPTS_DEFAULT);

    this.bgExecutorService = Preconditions.checkNotNull(bgExecutorService);
    this.downloader = Preconditions.checkNotNull(downloader);
  }

  @Override
  public final ListenableFuture<ListenableWorker.Result> startWork() {
    TcLog.d(
        TAG,
        String.format(
            "Start download: from %s to %s, attempt:%d",
            url, destinationPath, getRunAttemptCount()));
    if (getRunAttemptCount() >= maxDownloadAttempts) {
      TcLog.d(TAG, "Max attempt reached. Abort download task.");
      return Futures.immediateFuture(ListenableWorker.Result.failure());
    }

    File targetFile = new File(destinationPath);
    ListenableFuture<Long> downloadFuture =
        (reuseExistingFile && targetFile.exists())
            ? Futures.immediateFuture(targetFile.length())
            : downloader.download(URI.create(url), targetFile);

    return FluentFuture.from(downloadFuture)
        .transform(
            unusedBytesWritten -> {
              if (!targetFile.exists()) {
                throw new IllegalStateException("Download succeeded but target file not found.");
              }
              handleDownloadedFile(targetFile);
              return ListenableWorker.Result.success();
            },
            bgExecutorService)
        .catching(
            Throwable.class,
            e -> {
              TcLog.e(TAG, "Download attempt failed.", e);
              // Always delete downlaoded file if the work fails.
              targetFile.delete();
              // Retry until reach max allowed attempts (attempt starts from 0)
              // The backoff time between two tries will grow exponentially (i.e. 30s, 1min,
              // 2min, 4min). This is configurable when building the request.
              return ListenableWorker.Result.retry();
            },
            bgExecutorService);
  }

  /**
   * Subclass Workers should override (and only override) this method to handle downloaded file
   * (e.g. validation, rename). They should throw unchecked Exceptions if failure occurs.
   */
  abstract Void handleDownloadedFile(File downloadedFile);

  /**
   * This method will be called when we our work gets interrupted by the system. Result future
   * should have already been cancelled in that case. Unless it's because the REPLACE policy of
   * WorkManager unique queue, the interrupted work will be rescheduled later.
   */
  @Override
  public final void onStopped() {
    TcLog.d(
        TAG,
        String.format(
            "Stop download: from %s to %s, attempt:%d",
            url, destinationPath, getRunAttemptCount()));
    bgExecutorService.shutdown();
  }

  /**
   * Helper to create a base input Data builder.
   *
   * @param url the URL from where to download content
   * @param destinationPath the path on the device to store the downlaoded file
   * @param reuseExistingFile if True, we will skip the download if a file exists in destinationPath
   * @param maxDownloadAttempts max times to try before we abort this download task
   */
  static final Data.Builder createInputDataBuilder(
      String url, String destinationPath, boolean reuseExistingFile, int maxDownloadAttempts) {
    return new Data.Builder()
        .putString(DATA_URL_KEY, url)
        .putString(DATA_DESTINATION_PATH_KEY, destinationPath)
        .putBoolean(DATA_REUSE_EXISTING_FILE_KEY, reuseExistingFile)
        .putInt(DATA_MAX_DOWNLOAD_ATTEMPTS_KEY, maxDownloadAttempts);
  }
}
