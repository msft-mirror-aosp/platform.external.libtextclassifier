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

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.LocaleList;
import android.provider.DeviceConfig;
import android.text.TextUtils;
import androidx.work.BackoffPolicy;
import androidx.work.Constraints;
import androidx.work.ExistingWorkPolicy;
import androidx.work.ListenableWorker;
import androidx.work.NetworkType;
import androidx.work.OneTimeWorkRequest;
import androidx.work.WorkInfo;
import androidx.work.WorkManager;
import androidx.work.WorkQuery;
import com.android.textclassifier.common.ModelFileManager;
import com.android.textclassifier.common.ModelType;
import com.android.textclassifier.common.TextClassifierSettings;
import com.android.textclassifier.common.base.TcLog;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Enums;
import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableList;
import com.google.common.util.concurrent.ListeningExecutorService;
import java.time.Duration;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutionException;

/** Manager to listen to config update and download latest models. */
public final class ModelDownloadManager {
  private static final String TAG = "ModelDownloadManager";

  static final String UNIVERSAL_LOCALE_TAG = "universal";

  public static final int CHECK_REASON_TCS_ON_CREATE = 0;
  public static final int CHECK_REASON_DEVICE_CONFIG_UPDATED = 1;
  public static final int CHECK_REASON_LOCALE_UPDATED = 2;

  // Keep the records forever (100 Years). We use the record to skip previously failed tasks.
  static final long DAYS_TO_KEEP_THE_DOWNLOAD_RESULT = 365000L;

  private final Object lock = new Object();

  private final Context appContext;
  private final Class<? extends ListenableWorker> modelDownloadWorkerClass;
  private final ModelFileManager modelFileManager;
  private final TextClassifierSettings settings;
  private final ListeningExecutorService executorService;
  private final DeviceConfig.OnPropertiesChangedListener deviceConfigListener;
  private final BroadcastReceiver localeChangedReceiver;

  /**
   * Constructor for ModelDownloadManager.
   *
   * @param appContext the context of this application
   * @param modelFileManager ModelFileManager to interact with storage layer
   * @param settings TextClassifierSettings to access DeviceConfig and other settings
   * @param executorService background executor service
   */
  public ModelDownloadManager(
      Context appContext,
      ModelFileManager modelFileManager,
      TextClassifierSettings settings,
      ListeningExecutorService executorService) {
    this(appContext, NewModelDownloadWorker.class, modelFileManager, settings, executorService);
  }

  @VisibleForTesting
  ModelDownloadManager(
      Context appContext,
      Class<? extends ListenableWorker> modelDownloadWorkerClass,
      ModelFileManager modelFileManager,
      TextClassifierSettings settings,
      ListeningExecutorService executorService) {
    this.appContext = Preconditions.checkNotNull(appContext);
    this.modelDownloadWorkerClass = Preconditions.checkNotNull(modelDownloadWorkerClass);
    this.modelFileManager = Preconditions.checkNotNull(modelFileManager);
    this.settings = Preconditions.checkNotNull(settings);
    this.executorService = Preconditions.checkNotNull(executorService);

    this.deviceConfigListener =
        new DeviceConfig.OnPropertiesChangedListener() {
          @Override
          public void onPropertiesChanged(DeviceConfig.Properties unused) {
            // We will only be notified for changes in our package. Trigger the check even when the
            // change is unrelated just in case we missed a previous update.
            onTextClassifierDeviceConfigChanged();
          }
        };
    this.localeChangedReceiver =
        new BroadcastReceiver() {
          @Override
          public void onReceive(Context context, Intent intent) {
            onLocaleChanged();
          }
        };
  }

  /** Notifies the model downlaoder that the text classifier service is created. */
  public void onTextClassifierServiceCreated() {
    DeviceConfig.addOnPropertiesChangedListener(
        DeviceConfig.NAMESPACE_TEXTCLASSIFIER, executorService, deviceConfigListener);
    appContext.registerReceiver(
        localeChangedReceiver, new IntentFilter(Intent.ACTION_LOCALE_CHANGED));
    TcLog.d(TAG, "DeviceConfig listener and locale change listener are registered.");
    if (!settings.isModelDownloadManagerEnabled()) {
      return;
    }
    executorService.execute(
        () -> {
          TcLog.d(TAG, "Checking downloader flags because of the start of TextClassifierService.");
          modelFileManager.deleteUnusedModelFiles();
          checkConfigAndScheduleDownloads();
        });
  }

  // TODO(licha): Make this private. Let the constructor accept a receiver to enable testing.
  /** Notifies the model downlaoder that the system locale setting is changed. */
  @VisibleForTesting
  void onLocaleChanged() {
    if (!settings.isModelDownloadManagerEnabled()) {
      return;
    }
    executorService.execute(
        () -> {
          TcLog.d(TAG, "Checking downloader flags because of locale changes.");
          modelFileManager.deleteUnusedModelFiles();
          checkConfigAndScheduleDownloads();
        });
  }

  // TODO(licha): Make this private. Let the constructor accept a receiver to enable testing.
  /** Notifies the model downlaoder that the device config for textclassifier is changed. */
  @VisibleForTesting
  void onTextClassifierDeviceConfigChanged() {
    if (!settings.isModelDownloadManagerEnabled()) {
      return;
    }
    executorService.execute(
        () -> {
          TcLog.d(TAG, "Checking downloader flags because of device config changes.");
          checkConfigAndScheduleDownloads();
        });
  }

  /** Clean up internal states on destroying. */
  public void destroy() {
    DeviceConfig.removeOnPropertiesChangedListener(deviceConfigListener);
    appContext.unregisterReceiver(localeChangedReceiver);
    TcLog.d(TAG, "DeviceConfig and Locale listener unregistered by ModelDownloadeManager");
  }

  /**
   * Check DeviceConfig and schedule new model download requests synchronously. This method is
   * synchronized and contains blocking operations, only call it in a background executor.
   */
  private void checkConfigAndScheduleDownloads() {
    synchronized (lock) {
      WorkManager workManager = WorkManager.getInstance(appContext);
      List<Locale.LanguageRange> languageRanges =
          Locale.LanguageRange.parse(LocaleList.getAdjustedDefault().toLanguageTags());
      for (String modelType : ModelType.values()) {
        // Notice: Be careful of the Locale.lookupTag() matching logic: 1) it will convert the tag
        // to lower-case only; 2) it matches tags from tail to head and does not allow missing
        // pieces. E.g. if your system locale is zh-hans-cn, it won't match zh-cn.
        String bestTag =
            Locale.lookupTag(languageRanges, settings.getLanguageTagsForManifestURL(modelType));
        String localeTag = bestTag != null ? bestTag : UNIVERSAL_LOCALE_TAG;
        TcLog.v(TAG, String.format("Checking model type: %s, best tag: %s", modelType, localeTag));

        // One manifest url can uniquely identify a model in the world
        String manifestUrl = settings.getManifestURL(modelType, localeTag);
        if (TextUtils.isEmpty(manifestUrl)) {
          continue;
        }

        // Query the history for this url. Stop if we handled this url before.
        // TODO(licha): We may skip downloads incorrectly if we switch locales back and forth.
        WorkQuery workQuery = WorkQuery.Builder.fromTags(ImmutableList.of(manifestUrl)).build();
        try {
          List<WorkInfo> workInfos = workManager.getWorkInfos(workQuery).get();
          if (!workInfos.isEmpty()) {
            TcLog.v(
                TAG,
                String.format(
                    "Target manifest has an existing state of: %s. Skip.",
                    workInfos.get(0).getState().name()));
            continue;
          }
        } catch (ExecutionException | InterruptedException e) {
          TcLog.e(TAG, "Failed to query queued requests. Ignore and continue.", e);
        }

        NetworkType networkType =
            Enums.getIfPresent(NetworkType.class, settings.getManifestDownloadRequiredNetworkType())
                .or(NetworkType.UNMETERED);
        OneTimeWorkRequest downloadRequest =
            new OneTimeWorkRequest.Builder(modelDownloadWorkerClass)
                .setInputData(
                    NewModelDownloadWorker.createInputData(
                        modelType, localeTag, manifestUrl, settings.getModelDownloadMaxAttempts()))
                .addTag(manifestUrl)
                .setConstraints(
                    new Constraints.Builder()
                        .setRequiredNetworkType(networkType)
                        .setRequiresBatteryNotLow(true)
                        .setRequiresStorageNotLow(true)
                        .build())
                .setBackoffCriteria(
                    BackoffPolicy.EXPONENTIAL,
                    settings.getModelDownloadBackoffDelayInMillis(),
                    MILLISECONDS)
                .keepResultsForAtLeast(Duration.ofDays(DAYS_TO_KEEP_THE_DOWNLOAD_RESULT))
                .build();

        // When we enqueue a new request, existing pending request in the same queue will be
        // cancelled. With this, device will be able to abort previous unfinished downloads
        // (e.g. 711) when a fresher model is already(e.g. v712).
        try {
          // Block until we enqueue the request successfully (to avoid potential race condition)
          workManager
              .enqueueUniqueWork(
                  formatUniqueWorkName(modelType, localeTag),
                  ExistingWorkPolicy.REPLACE,
                  downloadRequest)
              .getResult()
              .get();
          TextClassifierDownloadLogger.downloadSceduled(modelType, manifestUrl);
          TcLog.d(TAG, "Download scheduled: " + manifestUrl);
        } catch (ExecutionException | InterruptedException e) {
          TextClassifierDownloadLogger.downloadFailedAndAbort(
              modelType,
              manifestUrl,
              ModelDownloadException.FAILED_TO_SCHEDULE,
              /* runAttemptCount= */ 0);
          TcLog.e(TAG, "Failed to enqueue a request", e);
        }
      }
    }
  }

  /** Formats unique work name for WorkManager. */
  static String formatUniqueWorkName(@ModelType.ModelTypeDef String modelType, String localeTag) {
    return String.format("%s-%s", modelType, localeTag);
  }
}
