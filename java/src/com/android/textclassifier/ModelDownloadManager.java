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

import android.os.LocaleList;
import android.provider.DeviceConfig;
import android.text.TextUtils;
import androidx.work.Constraints;
import androidx.work.ExistingWorkPolicy;
import androidx.work.ListenableWorker;
import androidx.work.OneTimeWorkRequest;
import androidx.work.WorkInfo;
import androidx.work.WorkManager;
import androidx.work.WorkQuery;
import com.android.textclassifier.ModelFileManager.ModelType;
import com.android.textclassifier.common.base.TcLog;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableList;
import com.google.common.util.concurrent.ListeningExecutorService;
import java.io.File;
import java.time.Duration;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutionException;

/** Manager to listen to config update and download latest models. */
final class ModelDownloadManager {
  private static final String TAG = "ModelDownloadManager";

  static final String UNIVERSAL_MODEL_LANGUAGE_TAG = "universal";
  static final String TEXT_CLASSIFIER_URL_PREFIX =
      "https://www.gstatic.com/android/text_classifier/";
  static final long DAYS_TO_KEEP_THE_DOWNLOAD_RESULT = 28L;

  private final Object lock = new Object();

  private final WorkManager workManager;
  private final Class<? extends ListenableWorker> manifestDownloadWorkerClass;
  private final ModelFileManager modelFileManager;
  private final TextClassifierSettings settings;
  private final ListeningExecutorService executorService;
  private final DeviceConfig.OnPropertiesChangedListener deviceConfigListener;

  /**
   * Constructor for ModelDownloadManager.
   *
   * @param workManager singleton WorkManager instance
   * @param manifestDownloadWorkerClass WorkManager's Worker class to download model manifest and
   *     schedule the actual model download work
   * @param modelFileManager ModelFileManager to interact with storage layer
   * @param settings TextClassifierSettings to access DeviceConfig and other settings
   * @param executorService background executor service
   */
  public ModelDownloadManager(
      WorkManager workManager,
      Class<? extends ListenableWorker> manifestDownloadWorkerClass,
      ModelFileManager modelFileManager,
      TextClassifierSettings settings,
      ListeningExecutorService executorService) {
    this.workManager = Preconditions.checkNotNull(workManager);
    this.manifestDownloadWorkerClass = Preconditions.checkNotNull(manifestDownloadWorkerClass);
    this.modelFileManager = Preconditions.checkNotNull(modelFileManager);
    this.settings = Preconditions.checkNotNull(settings);
    this.executorService = Preconditions.checkNotNull(executorService);

    this.deviceConfigListener =
        new DeviceConfig.OnPropertiesChangedListener() {
          @Override
          public void onPropertiesChanged(DeviceConfig.Properties unused) {
            // Trigger the check even when the change is unrelated just in case we missed a previous
            // update
            checkConfigAndScheduleDownloads();
          }
        };
  }

  /**
   * Registers a listener to related DeviceConfig flag changes. Will also download models with
   * {@code executorService} if necessary.
   */
  public void init() {
    DeviceConfig.addOnPropertiesChangedListener(
        DeviceConfig.NAMESPACE_TEXTCLASSIFIER, executorService, deviceConfigListener);
    TcLog.d(TAG, "DeviceConfig listener registered by ModelDownloadManager");
    // Check flags in background, in case any updates heppened before the TCS starts
    executorService.execute(this::checkConfigAndScheduleDownloads);
  }

  /** Un-register the listener to DeviceConfig. */
  public void destroy() {
    DeviceConfig.removeOnPropertiesChangedListener(deviceConfigListener);
    TcLog.d(TAG, "DeviceConfig listener unregistered by ModelDownloadeManager");
  }

  /**
   * Check DeviceConfig and schedule new model download requests synchronously. This method is
   * synchronized and contains blocking operations, only call it in a background executor.
   */
  private void checkConfigAndScheduleDownloads() {
    TcLog.v(TAG, "Checking DeviceConfig to see whether there are new models to download");
    synchronized (lock) {
      List<Locale.LanguageRange> languageRanges =
          Locale.LanguageRange.parse(LocaleList.getAdjustedDefault().toLanguageTags());
      for (String modelType : ModelType.values()) {
        // Notice: Be careful of the Locale.lookupTag() matching logic: 1) it will convert the tag
        // to lower-case only; 2) it matches tags from tail to head and does not allow missing
        // pieces. E.g. if your system locale is zh-hans-cn, it won't match zh-cn.
        String bestTag =
            Locale.lookupTag(
                languageRanges, settings.getLanguageTagsForManifestURLSuffix(modelType));
        String modelLanguageTag = bestTag != null ? bestTag : UNIVERSAL_MODEL_LANGUAGE_TAG;

        // One manifest url suffix can uniquely identify a model in the world
        String manifestUrlSuffix = settings.getManifestURLSuffix(modelType, modelLanguageTag);
        if (TextUtils.isEmpty(manifestUrlSuffix)) {
          continue;
        }
        String manifestUrl = TEXT_CLASSIFIER_URL_PREFIX + manifestUrlSuffix;

        // Check whether a manifest or a model is in the queue/in the middle of downloading. Both
        // manifest/model works are tagged with the manifest URL.
        WorkQuery workQuery =
            WorkQuery.Builder.fromTags(ImmutableList.of(manifestUrl))
                .addStates(
                    Arrays.asList(
                        WorkInfo.State.BLOCKED, WorkInfo.State.ENQUEUED, WorkInfo.State.RUNNING))
                .build();
        try {
          List<WorkInfo> workInfos = workManager.getWorkInfos(workQuery).get();
          if (!workInfos.isEmpty()) {
            TcLog.v(TAG, "Target model is already in the download queue.");
            continue;
          }
        } catch (ExecutionException | InterruptedException e) {
          TcLog.e(TAG, "Failed to query queued requests. Ignore and continue.", e);
        }

        // Target file's name has the url suffix encoded in it
        File targetModelFile = modelFileManager.getDownloadTargetFile(modelType, manifestUrlSuffix);
        if (!targetModelFile.getParentFile().exists()) {
          if (!targetModelFile.getParentFile().mkdirs()) {
            TcLog.e(TAG, "Failed to create " + targetModelFile.getParentFile().getAbsolutePath());
            continue;
          }
        }
        // TODO(licha): Ideally, we should verify whether the existing file can be loaded
        // successfully
        // Notes: We also don't check factory models and models downloaded by ConfigUpdater. But
        // this is probablly fine because it's unlikely to have an overlap.
        if (targetModelFile.exists()) {
          TcLog.v(TAG, "Target model is already in the storage.");
          continue;
        }

        // Skip models downloaded successfully in (at least) past DAYS_TO_KEEP_THE_DOWNLOAD_RESULT
        // Because we delete less-preferred models after one model downloaded, it's possible that
        // we fall in a loop (download - delete - download again) if P/H flag is in a bad state.
        // NOTICE: Because we use an unique work name here, if we download model-1 first and then
        // model-2, then model-1's WorkInfo will be lost. In that case, if the flag goes back to
        // model-1, we will download it again even if it's within DAYS_TO_KEEP_THE_DOWNLOAD_RESULT
        WorkQuery downlaodedBeforeWorkQuery =
            WorkQuery.Builder.fromTags(ImmutableList.of(manifestUrl))
                .addStates(ImmutableList.of(WorkInfo.State.SUCCEEDED))
                .addUniqueWorkNames(
                    ImmutableList.of(getModelUniqueWorkName(modelType, modelLanguageTag)))
                .build();
        try {
          List<WorkInfo> downloadedBeforeWorkInfos =
              workManager.getWorkInfos(downlaodedBeforeWorkQuery).get();
          if (!downloadedBeforeWorkInfos.isEmpty()) {
            TcLog.v(TAG, "The model was downloaded successfully before and got cleaned-up later");
            continue;
          }
        } catch (ExecutionException | InterruptedException e) {
          TcLog.e(TAG, "Failed to query queued requests. Ignore and continue.", e);
        }

        String targetModelPath = targetModelFile.getAbsolutePath();
        String targetManifestPath = getTargetManifestPath(targetModelPath);
        OneTimeWorkRequest manifestDownloadRequest =
            new OneTimeWorkRequest.Builder(manifestDownloadWorkerClass)
                .setInputData(
                    ManifestDownloadWorker.createInputData(
                        modelType,
                        modelLanguageTag,
                        manifestUrl,
                        targetManifestPath,
                        targetModelPath,
                        settings.getModelDownloadMaxAttempts(),
                        /* reuseExistingManifestFile= */ true))
                .addTag(manifestUrl)
                .setConstraints(
                    new Constraints.Builder()
                        .setRequiredNetworkType(settings.getManifestDownloadRequiredNetworkType())
                        .setRequiresBatteryNotLow(true)
                        .setRequiresStorageNotLow(true)
                        .build())
                .keepResultsForAtLeast(Duration.ofDays(DAYS_TO_KEEP_THE_DOWNLOAD_RESULT))
                .build();

        // When we enqueue a new request, existing pending request in the same queue will be
        // cancelled. With this, device will be able to abort previous unfinished downloads
        // (e.g. 711) when a fresher model is already(e.g. v712).
        try {
          // Block until we enqueue the request successfully
          workManager
              .enqueueUniqueWork(
                  getManifestUniqueWorkName(modelType, modelLanguageTag),
                  ExistingWorkPolicy.REPLACE,
                  manifestDownloadRequest)
              .getResult()
              .get();
          TcLog.d(TAG, "Download scheduled: " + manifestUrl);
        } catch (ExecutionException | InterruptedException e) {
          TcLog.e(TAG, "Failed to enqueue a request", e);
        }
      }
    }
  }

  @VisibleForTesting
  void checkConfigAndScheduleDownloadsForTesting() {
    checkConfigAndScheduleDownloads();
  }

  @VisibleForTesting
  static String getTargetManifestPath(String targetModelPath) {
    return targetModelPath + ".manifest";
  }

  @VisibleForTesting
  static String getManifestUniqueWorkName(
      @ModelType.ModelTypeDef String modelType, String modelLanguageTag) {
    return String.format("manifest-%s-%s", modelType, modelLanguageTag);
  }

  // ManifestDownloadWorker needs to access this
  static String getModelUniqueWorkName(
      @ModelType.ModelTypeDef String modelType, String modelLanguageTag) {
    return "model-" + modelType + "-" + modelLanguageTag;
  }
}
