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
import androidx.work.Constraints;
import androidx.work.Data;
import androidx.work.ExistingWorkPolicy;
import androidx.work.ListenableWorker;
import androidx.work.NetworkType;
import androidx.work.OneTimeWorkRequest;
import androidx.work.WorkManager;
import androidx.work.WorkerParameters;
import com.android.textclassifier.ModelFileManager.ModelType;
import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.protobuf.ExtensionRegistryLite;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.collect.EnumBiMap;
import com.google.common.collect.ImmutableMap;
import com.google.common.util.concurrent.Futures;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.time.Duration;

/** Worker to download/parse models' manifest file and schedule the acutal model download task. */
public final class ManifestDownloadWorker extends AbstractDownloadWorker {
  private static final String TAG = "ManifestDownloadWorker";
  private static final String DATA_MODEL_TYPE_KEY = "ManifestDownloadWorker_modelType";
  private static final String DATA_MODEL_LANGUAGE_TAG_KEY =
      "ManifestDownloadWorker_modelLanguageTag";
  private static final String DATA_MANIFEST_URL_KEY = "ManifestDownloadWorker_manifestUrl";
  private static final String DATA_TARGET_MODEL_PATH_KEY = "ManifestDownloadWorker_targetModelPath";

  private static final EnumBiMap<ModelManifest.NetworkType, NetworkType> NETWORK_TYPE_MAP =
      EnumBiMap.create(
          ImmutableMap.of(
              ModelManifest.NetworkType.UNMETERED, NetworkType.UNMETERED,
              ModelManifest.NetworkType.METERED, NetworkType.METERED,
              ModelManifest.NetworkType.NOT_REQUIRED, NetworkType.NOT_REQUIRED,
              ModelManifest.NetworkType.NOT_ROAMING, NetworkType.NOT_ROAMING,
              ModelManifest.NetworkType.CONNECTED, NetworkType.CONNECTED));

  private final String modelType;
  private final String modelLanguageTag;
  private final String manifestUrl;
  private final String targetModelPath;

  private final Context context;
  private final Class<? extends ListenableWorker> modelDownloadWorkerClass;
  private final WorkManager workManager;

  public ManifestDownloadWorker(Context context, WorkerParameters workerParams) {
    this(context, workerParams, ModelDownloadWorker.class);
  }

  @VisibleForTesting
  ManifestDownloadWorker(
      Context context,
      WorkerParameters workerParams,
      Class<? extends ListenableWorker> modelDownloadWorkerClass) {
    super(context, workerParams);

    this.modelType = Preconditions.checkNotNull(getInputData().getString(DATA_MODEL_TYPE_KEY));
    this.modelLanguageTag =
        Preconditions.checkNotNull(getInputData().getString(DATA_MODEL_LANGUAGE_TAG_KEY));
    this.manifestUrl = Preconditions.checkNotNull(getInputData().getString(DATA_MANIFEST_URL_KEY));
    this.targetModelPath =
        Preconditions.checkNotNull(getInputData().getString(DATA_TARGET_MODEL_PATH_KEY));

    this.context = Preconditions.checkNotNull(context);
    this.modelDownloadWorkerClass = Preconditions.checkNotNull(modelDownloadWorkerClass);
    this.workManager = Preconditions.checkNotNull(WorkManager.getInstance(context));
  }

  @Override
  public Void handleDownloadedFile(File manifestFile) {
    TcLog.d(TAG, "Start to parse model manifest: " + manifestFile.getAbsolutePath());
    ModelManifest modelManifest;
    try {
      modelManifest =
          ModelManifest.parseFrom(
              new FileInputStream(manifestFile), ExtensionRegistryLite.getEmptyRegistry());
    } catch (IOException e) {
      throw new IllegalStateException("Failed to parse the manifest file.", e);
    }

    Preconditions.checkState(modelManifest.getModelsCount() == 1);
    ModelManifest.Model model = modelManifest.getModels(0);
    Preconditions.checkState(
        model.getUrl().startsWith(ModelDownloadManager.TEXT_CLASSIFIER_URL_PREFIX));
    Preconditions.checkState(model.getSizeInBytes() > 0 && !model.getFingerprint().isEmpty());

    File targetModelFile = new File(targetModelPath);
    File pendingModelFile = new File(context.getCacheDir(), targetModelFile.getName() + ".pending");
    OneTimeWorkRequest modelDownloadRequest =
        new OneTimeWorkRequest.Builder(modelDownloadWorkerClass)
            .setInputData(
                ModelDownloadWorker.createInputData(
                    model.getUrl(),
                    model.getSizeInBytes(),
                    model.getFingerprint(),
                    manifestFile.getAbsolutePath(),
                    pendingModelFile.getAbsolutePath(),
                    targetModelPath,
                    /* maxDownloadAttempts= */ 5,
                    /* reuseExistingModelFile= */ false))
            .addTag(manifestUrl)
            .setConstraints(
                new Constraints.Builder()
                    .setRequiredNetworkType(
                        NETWORK_TYPE_MAP.get(modelManifest.getRequiredNetworkType()))
                    .setRequiresBatteryNotLow(modelManifest.getRequiresBatteryNotLow())
                    .setRequiresCharging(modelManifest.getRequiresCharging())
                    .setRequiresDeviceIdle(modelManifest.getRequiresDeviceIdle())
                    .setRequiresStorageNotLow(modelManifest.getRequiresStorageNotLow())
                    .build())
            .keepResultsForAtLeast(
                Duration.ofDays(ModelDownloadManager.DAYS_TO_KEEP_THE_DOWNLOAD_RESULT))
            .build();

    // Enqueue chained requests to a unique queue (different from the manifest queue)
    Futures.getUnchecked(
        workManager
            .enqueueUniqueWork(
                ModelDownloadManager.getModelUniqueWorkName(modelType, modelLanguageTag),
                ExistingWorkPolicy.REPLACE,
                modelDownloadRequest)
            .getResult());
    return null;
  }

  /** Creates input Data for a ManifestDownloadWorker. */
  public static Data createInputData(
      @ModelType.ModelTypeDef String modelType,
      String modelLanguageTag,
      String manifestUrl,
      String targetManifestPath,
      String targetModelPath,
      int maxDownloadAttempts,
      boolean reuseExistingManifestFile) {
    return AbstractDownloadWorker.createInputDataBuilder(
            manifestUrl, targetManifestPath, reuseExistingManifestFile, maxDownloadAttempts)
        .putString(DATA_MODEL_TYPE_KEY, modelType)
        .putString(DATA_MODEL_LANGUAGE_TAG_KEY, modelLanguageTag)
        .putString(DATA_MANIFEST_URL_KEY, manifestUrl)
        .putString(DATA_TARGET_MODEL_PATH_KEY, targetModelPath)
        .build();
  }
}
