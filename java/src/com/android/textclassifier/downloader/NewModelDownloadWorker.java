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
import android.util.ArrayMap;
import android.util.Pair;
import androidx.work.ListenableWorker;
import androidx.work.WorkerParameters;
import com.android.textclassifier.common.ModelType;
import com.android.textclassifier.common.ModelType.ModelTypeDef;
import com.android.textclassifier.common.TextClassifierServiceExecutors;
import com.android.textclassifier.common.TextClassifierSettings;
import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.downloader.DownloadedModelDatabase.Manifest;
import com.android.textclassifier.downloader.DownloadedModelDatabase.ManifestEnrollment;
import com.android.textclassifier.downloader.DownloadedModelDatabase.Model;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableMap;
import com.google.common.util.concurrent.FluentFuture;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.ListeningExecutorService;
import com.google.errorprone.annotations.concurrent.GuardedBy;
import java.util.ArrayList;
import java.util.Locale;

// TODO(licha): Rename this to ModelDownloadWorker.
/** The WorkManager worker to download models for TextClassifierService. */
public final class NewModelDownloadWorker extends ListenableWorker {
  private static final String TAG = "NewModelDownloadWorker";

  private final ListeningExecutorService executorService;
  private final ModelDownloader downloader;
  private final DownloadedModelManager downloadedModelManager;
  private final TextClassifierSettings settings;

  private final Object lock = new Object();

  @GuardedBy("lock")
  private final ArrayMap<String, ListenableFuture<Void>> pendingDownloads;

  private ImmutableMap<String, Pair<String, String>> modelTypeToLocaleTagAndManifestUrls;

  public NewModelDownloadWorker(Context context, WorkerParameters workerParams) {
    super(context, workerParams);
    this.executorService = TextClassifierServiceExecutors.getDownloaderExecutor();
    this.downloader = new ModelDownloaderImpl(context, executorService);
    this.downloadedModelManager = DownloadedModelManagerImpl.getInstance(context);
    this.settings = new TextClassifierSettings();
    this.pendingDownloads = new ArrayMap<>();
    this.modelTypeToLocaleTagAndManifestUrls = null;
  }

  @VisibleForTesting
  NewModelDownloadWorker(
      Context context,
      WorkerParameters workerParams,
      ListeningExecutorService executorService,
      ModelDownloader modelDownloader,
      DownloadedModelManager downloadedModelManager,
      TextClassifierSettings settings) {
    super(context, workerParams);
    this.executorService = executorService;
    this.downloader = modelDownloader;
    this.downloadedModelManager = downloadedModelManager;
    this.settings = settings;
    this.pendingDownloads = new ArrayMap<>();
    this.modelTypeToLocaleTagAndManifestUrls = null;
  }

  @Override
  public final ListenableFuture<ListenableWorker.Result> startWork() {
    // Notice: startWork() is invoked on the main thread
    if (!settings.isModelDownloadManagerEnabled()) {
      TcLog.e(TAG, "Model Downloader is disabled. Abort the work.");
      return Futures.immediateFuture(ListenableWorker.Result.failure());
    }
    TcLog.v(TAG, "Start download work...");
    if (getRunAttemptCount() >= settings.getModelDownloadWorkerMaxAttempts()) {
      TcLog.d(TAG, "Max attempt reached. Abort download work.");
      return Futures.immediateFuture(ListenableWorker.Result.failure());
    }

    return FluentFuture.from(Futures.submitAsync(this::checkAndDownloadModels, executorService))
        .transform(
            allSucceeded -> {
              Preconditions.checkNotNull(modelTypeToLocaleTagAndManifestUrls);
              downloadedModelManager.onDownloadCompleted(modelTypeToLocaleTagAndManifestUrls);
              TcLog.v(TAG, "Download work completed. Succeeded: " + allSucceeded);
              return allSucceeded
                  ? ListenableWorker.Result.success()
                  : ListenableWorker.Result.retry();
            },
            executorService)
        .catching(
            Throwable.class,
            t -> {
              TcLog.e(TAG, "Unexpected Exception during downloading: ", t);
              return ListenableWorker.Result.retry();
            },
            executorService);
  }

  /**
   * Check device config and dispatch download tasks for all modelTypes.
   *
   * <p>Download tasks will be combined and logged after completion. Return true if all tasks
   * succeeded
   */
  private ListenableFuture<Boolean> checkAndDownloadModels() {
    Locale primaryLocale = Locale.getDefault();
    ArrayList<ListenableFuture<Boolean>> downloadResultFutures = new ArrayList<>();
    ImmutableMap.Builder<String, Pair<String, String>> modelTypeToLocaleTagAndManifestUrlsBuilder =
        ImmutableMap.builder();
    for (String modelType : ModelType.values()) {
      Pair<String, String> bestLocaleTagAndManifestUrl =
          LocaleUtils.lookupBestLocaleTagAndManifestUrl(modelType, primaryLocale, settings);
      if (bestLocaleTagAndManifestUrl == null) {
        TcLog.w(
            TAG,
            String.format(
                "No suitable manifest for %s, %s", modelType, primaryLocale.toLanguageTag()));
        continue;
      }
      modelTypeToLocaleTagAndManifestUrlsBuilder.put(modelType, bestLocaleTagAndManifestUrl);
      String bestLocaleTag = bestLocaleTagAndManifestUrl.first;
      String manifestUrl = bestLocaleTagAndManifestUrl.second;
      TcLog.v(
          TAG,
          String.format(
              "model type: %s, device locale tag: %s, best locale tag: %s, manifest url: %s",
              modelType, primaryLocale.toLanguageTag(), bestLocaleTag, manifestUrl));
      if (!shouldDownloadManifest(modelType, bestLocaleTag, manifestUrl)) {
        continue;
      }
      downloadResultFutures.add(downloadManifestAndRegister(modelType, bestLocaleTag, manifestUrl));
    }
    modelTypeToLocaleTagAndManifestUrls = modelTypeToLocaleTagAndManifestUrlsBuilder.build();

    return Futures.whenAllComplete(downloadResultFutures)
        .call(
            () -> {
              TcLog.v(TAG, "All Download Tasks Completed");
              boolean allSucceeded = true;
              for (ListenableFuture<Boolean> downloadResultFuture : downloadResultFutures) {
                allSucceeded &= Futures.getDone(downloadResultFuture);
              }
              return allSucceeded;
            },
            executorService);
  }

  private boolean shouldDownloadManifest(
      @ModelTypeDef String modelType, String localeTag, String manifestUrl) {
    Manifest downloadedManifest = downloadedModelManager.getManifest(manifestUrl);
    if (downloadedManifest == null) {
      return true;
    }
    if (downloadedManifest.getStatus() == Manifest.STATUS_FAILED) {
      if (downloadedManifest.getFailureCounts() >= settings.getManifestDownloadMaxAttempts()) {
        TcLog.w(
            TAG,
            String.format(
                "Manifest failed too many times, stop retrying: %s %d",
                manifestUrl, downloadedManifest.getFailureCounts()));
        return false;
      } else {
        return true;
      }
    }
    ManifestEnrollment manifestEnrollment =
        downloadedModelManager.getManifestEnrollment(modelType, localeTag);
    return manifestEnrollment == null || !manifestUrl.equals(manifestEnrollment.getManifestUrl());
  }

  /**
   * Downloads a single manifest and models configured inside it.
   *
   * <p>The returned future should always resolve to a ManifestDownloadResult as we catch all
   * exceptions.
   */
  private ListenableFuture<Boolean> downloadManifestAndRegister(
      @ModelTypeDef String modelType, String localeTag, String manifestUrl) {
    return FluentFuture.from(downloadManifest(manifestUrl))
        .transform(
            unused -> {
              downloadedModelManager.registerManifestEnrollment(modelType, localeTag, manifestUrl);
              TextClassifierDownloadLogger.downloadSucceeded(
                  modelType, manifestUrl, getRunAttemptCount());
              TcLog.v(TAG, "Manifest downloaded and registered: " + manifestUrl);
              return true;
            },
            executorService)
        .catching(
            Throwable.class,
            t -> {
              downloadedModelManager.registerManifestDownloadFailure(manifestUrl);
              int errorCode = ModelDownloadException.UNKNOWN_FAILURE_REASON;
              if (t instanceof ModelDownloadException) {
                errorCode = ((ModelDownloadException) t).getErrorCode();
              }
              TcLog.e(TAG, "Failed to download manfiest: " + manifestUrl, t);
              TextClassifierDownloadLogger.downloadFailedAndRetry(
                  modelType, manifestUrl, errorCode, getRunAttemptCount());
              return false;
            },
            executorService);
  }

  // Download a manifest and its models, and register it to Manifest table.
  private ListenableFuture<Void> downloadManifest(String manifestUrl) {
    synchronized (lock) {
      Manifest downloadedManifest = downloadedModelManager.getManifest(manifestUrl);
      if (downloadedManifest != null
          && downloadedManifest.getStatus() == Manifest.STATUS_SUCCEEDED) {
        TcLog.v(TAG, "Manifest already downloaded: " + manifestUrl);
        return Futures.immediateVoidFuture();
      }
      if (pendingDownloads.containsKey(manifestUrl)) {
        return pendingDownloads.get(manifestUrl);
      }
      ListenableFuture<Void> manfiestDownloadFuture =
          FluentFuture.from(downloader.downloadManifest(manifestUrl))
              .transformAsync(
                  manifest -> {
                    ModelManifest.Model modelInfo = manifest.getModels(0);
                    return Futures.transform(
                        downloadModel(modelInfo), unused -> modelInfo, executorService);
                  },
                  executorService)
              .transform(
                  modelInfo -> {
                    downloadedModelManager.registerManifest(manifestUrl, modelInfo.getUrl());
                    return null;
                  },
                  executorService);
      pendingDownloads.put(manifestUrl, manfiestDownloadFuture);
      return manfiestDownloadFuture;
    }
  }
  // Download a model and register it into Model table.
  private ListenableFuture<Void> downloadModel(ModelManifest.Model modelInfo) {
    String modelUrl = modelInfo.getUrl();
    synchronized (lock) {
      Model downloadedModel = downloadedModelManager.getModel(modelUrl);
      if (downloadedModel != null) {
        TcLog.d(TAG, "Model file already exists: " + downloadedModel.getModelPath());
        return Futures.immediateVoidFuture();
      }
      if (pendingDownloads.containsKey(modelUrl)) {
        return pendingDownloads.get(modelUrl);
      }
      ListenableFuture<Void> modelDownloadFuture =
          FluentFuture.from(
                  downloader.downloadModel(
                      downloadedModelManager.getModelDownloaderDir(), modelInfo))
              .transform(
                  modelFile -> {
                    downloadedModelManager.registerModel(modelUrl, modelFile.getAbsolutePath());
                    TcLog.v(TAG, "Model File downloaded: " + modelUrl);
                    return null;
                  },
                  executorService);
      pendingDownloads.put(modelUrl, modelDownloadFuture);
      return modelDownloadFuture;
    }
  }

  /**
   * This method will be called when we our work gets interrupted by the system. Result future
   * should have already been cancelled in that case. Unless it's because the REPLACE policy of
   * WorkManager unique queue, the interrupted work will be rescheduled later.
   */
  @Override
  public final void onStopped() {
    TcLog.d(TAG, String.format("Stop download. Attempt:%d", getRunAttemptCount()));
  }
}
