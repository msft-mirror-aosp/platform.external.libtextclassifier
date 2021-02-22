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
import androidx.work.WorkerParameters;
import com.android.textclassifier.common.base.TcLog;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.hash.HashCode;
import com.google.common.hash.Hashing;
import com.google.common.io.Files;
import java.io.File;
import java.io.IOException;
import java.nio.file.StandardCopyOption;

/** Worker to download, validate and update model image files. */
public final class ModelDownloadWorker extends AbstractDownloadWorker {
  private static final String TAG = "ModelDownloadWorker";

  @VisibleForTesting
  static final String DATA_MANIFEST_PATH_KEY = "ModelDownloadWorker_manifestPath";

  @VisibleForTesting
  static final String DATA_TARGET_MODEL_PATH_KEY = "ModelDownloadWorker_targetModelPath";

  @VisibleForTesting
  static final String DATA_MODEL_SIZE_IN_BYTES_KEY = "ModelDownloadWorker_modelSizeInBytes";

  @VisibleForTesting
  static final String DATA_MODEL_FINGERPRINT_KEY = "ModelDownloadWorker_modelFingerprint";

  private final String manifestPath;
  private final String targetModelPath;
  private final long modelSizeInBytes;
  private final String modelFingerprint;
  private final ModelFileManager modelFileManager;

  public ModelDownloadWorker(Context context, WorkerParameters workerParams) {
    super(context, workerParams);
    this.manifestPath =
        Preconditions.checkNotNull(getInputData().getString(DATA_MANIFEST_PATH_KEY));
    this.targetModelPath =
        Preconditions.checkNotNull(getInputData().getString(DATA_TARGET_MODEL_PATH_KEY));
    this.modelSizeInBytes =
        getInputData().getLong(DATA_MODEL_SIZE_IN_BYTES_KEY, /* defaultValue= */ 0L);
    this.modelFingerprint =
        Preconditions.checkNotNull(getInputData().getString(DATA_MODEL_FINGERPRINT_KEY));
    this.modelFileManager = new ModelFileManager(context, new TextClassifierSettings());
  }

  @Override
  public Void handleDownloadedFile(File pendingModelFile) {
    TcLog.d(TAG, "Start to check pending model file: " + pendingModelFile.getAbsolutePath());
    try {
      validateModel(pendingModelFile, modelSizeInBytes, modelFingerprint);

      File targetModelFile = new File(targetModelPath);
      java.nio.file.Files.move(
          pendingModelFile.toPath(),
          targetModelFile.toPath(),
          StandardCopyOption.ATOMIC_MOVE,
          StandardCopyOption.REPLACE_EXISTING);
      TcLog.d(TAG, "Model file downloaded successfully: " + targetModelFile.getAbsolutePath());

      // Clean up manifest and older models
      new File(manifestPath).delete();
      modelFileManager.deleteUnusedModelFiles();
      return null;
    } catch (Exception e) {
      throw new IllegalStateException("Failed to validate or move pending model file.", e);
    } finally {
      pendingModelFile.delete();
    }
  }

  /** Model verification. Throws unchecked Exceptions if validation fails. */
  private static void validateModel(File pendingModelFile, long sizeInBytes, String fingerprint)
      throws IOException {
    if (!pendingModelFile.exists()) {
      throw new IllegalStateException("PendingModelFile does not exist.");
    }
    if (pendingModelFile.length() != sizeInBytes) {
      throw new IllegalStateException(
          String.format(
              "PendingModelFile size does not match: expected [%d] actual [%d]",
              sizeInBytes, pendingModelFile.length()));
    }
    HashCode pendingModelFingerprint = Files.asByteSource(pendingModelFile).hash(Hashing.sha384());
    if (!pendingModelFingerprint.equals(HashCode.fromString(fingerprint))) {
      throw new IllegalStateException(
          String.format(
              "PendingModelFile fingerprint does not match: expected [%s] actual [%s]",
              fingerprint, pendingModelFingerprint));
    }
    TcLog.d(TAG, "Pending model file passed validation.");
  }

  /** Creates input Data for a ModelDownloadWorker. */
  public static Data createInputData(
      String modelUrl,
      long modelSizeInBytes,
      String modelFingerprint,
      String manifestPath,
      String pendingModelPath,
      String targetModelPath,
      int maxDownloadAttempts,
      boolean reuseExistingModelFile) {
    return AbstractDownloadWorker.createInputDataBuilder(
            modelUrl, pendingModelPath, reuseExistingModelFile, maxDownloadAttempts)
        .putString(DATA_MANIFEST_PATH_KEY, manifestPath)
        .putString(DATA_TARGET_MODEL_PATH_KEY, targetModelPath)
        .putLong(DATA_MODEL_SIZE_IN_BYTES_KEY, modelSizeInBytes)
        .putString(DATA_MODEL_FINGERPRINT_KEY, modelFingerprint)
        .build();
  }
}
