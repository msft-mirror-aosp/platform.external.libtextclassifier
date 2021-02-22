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

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import androidx.work.ListenableWorker;
import androidx.work.WorkInfo;
import androidx.work.WorkManager;
import androidx.work.WorkerFactory;
import androidx.work.WorkerParameters;
import androidx.work.testing.TestDriver;
import androidx.work.testing.TestListenableWorkerBuilder;
import androidx.work.testing.WorkManagerTestInitHelper;
import com.android.textclassifier.ModelFileManager.ModelType;
import java.io.File;
import java.nio.file.Files;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class ManifestDownloadWorkerTest {
  private static final String MODEL_URL =
      "https://www.gstatic.com/android/text_classifier/q/v711/en.fb";
  private static final long MODEL_SIZE_IN_BYTES = 1L;
  private static final String MODEL_FINGERPRINT = "hash_fingerprint";
  private static final String MANIFEST_URL =
      "https://www.gstatic.com/android/text_classifier/q/v711/en.fb.manifest";
  private static final String TARGET_MODEL_PATH = "/not_used_fake_path.fb";
  private static final String MODEL_TYPE = ModelType.ANNOTATOR;
  private static final String MODEL_LANGUAGE_TAG = "en";
  private static final String WORK_MANAGER_UNIQUE_WORK_NAME =
      ModelDownloadManager.getModelUniqueWorkName(MODEL_TYPE, MODEL_LANGUAGE_TAG);
  private static final int WORKER_MAX_DOWNLOAD_ATTEMPTS = 5;
  private static final ModelManifest MODEL_MANIFEST_PROTO =
      ModelManifest.newBuilder()
          .addModels(
              ModelManifest.Model.newBuilder()
                  .setUrl(MODEL_URL)
                  .setSizeInBytes(MODEL_SIZE_IN_BYTES)
                  .setFingerprint(MODEL_FINGERPRINT)
                  .build())
          .build();

  private File manifestFile;
  private WorkManager workManager;
  private TestDriver workManagerTestDriver;

  @Before
  public void setUp() {
    Context context = ApplicationProvider.getApplicationContext();
    WorkManagerTestInitHelper.initializeTestWorkManager(context);

    this.manifestFile = new File(context.getCacheDir(), "model.fb.manifest");
    this.workManager = WorkManager.getInstance(context);
    this.workManagerTestDriver = WorkManagerTestInitHelper.getTestDriver(context);

    manifestFile.deleteOnExit();
  }

  @Test
  public void enqueueSuccessfullyAndCheckData() throws Exception {
    ManifestDownloadWorker worker = createWorker(MANIFEST_URL, manifestFile.getAbsolutePath());

    // We only want to test the downloaded file handling code, so reuse existing manifest file
    manifestFile.createNewFile();
    Files.write(manifestFile.toPath(), MODEL_MANIFEST_PROTO.toByteArray());
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(manifestFile.exists()).isTrue();

    WorkInfo workInfo =
        DownloaderTestUtils.queryTheOnlyWorkInfo(workManager, WORK_MANAGER_UNIQUE_WORK_NAME);
    assertThat(workInfo).isNotNull();
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(workInfo.getTags()).contains(MANIFEST_URL);

    // Check input Data with TestWorker
    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.SUCCESS);
    workManagerTestDriver.setAllConstraintsMet(workInfo.getId());

    WorkInfo newWorkInfo =
        DownloaderTestUtils.queryTheOnlyWorkInfo(workManager, WORK_MANAGER_UNIQUE_WORK_NAME);
    assertThat(newWorkInfo.getId()).isEqualTo(workInfo.getId());
    assertThat(newWorkInfo.getState()).isEqualTo(WorkInfo.State.SUCCEEDED);
    assertThat(newWorkInfo.getOutputData().getString(AbstractDownloadWorker.DATA_URL_KEY))
        .isEqualTo(MODEL_URL);
    assertThat(
            newWorkInfo
                .getOutputData()
                .getLong(ModelDownloadWorker.DATA_MODEL_SIZE_IN_BYTES_KEY, /* defaultValue= */ -1))
        .isEqualTo(MODEL_SIZE_IN_BYTES);
    assertThat(
            newWorkInfo.getOutputData().getString(ModelDownloadWorker.DATA_MODEL_FINGERPRINT_KEY))
        .isEqualTo(MODEL_FINGERPRINT);
    assertThat(
            newWorkInfo.getOutputData().getString(ModelDownloadWorker.DATA_TARGET_MODEL_PATH_KEY))
        .isEqualTo(TARGET_MODEL_PATH);
  }

  @Test
  public void invalidManifestFile_invalidFileDeletedAndRetry() throws Exception {
    ManifestDownloadWorker worker = createWorker(MANIFEST_URL, manifestFile.getAbsolutePath());

    manifestFile.createNewFile();
    Files.write(manifestFile.toPath(), "random_content".getBytes());
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    assertThat(manifestFile.exists()).isFalse();
  }

  private static ManifestDownloadWorker createWorker(String manifestUrl, String manifestPath) {
    return TestListenableWorkerBuilder.from(
            ApplicationProvider.getApplicationContext(), ManifestDownloadWorker.class)
        .setInputData(
            ManifestDownloadWorker.createInputData(
                MODEL_TYPE,
                MODEL_LANGUAGE_TAG,
                manifestUrl,
                manifestPath,
                TARGET_MODEL_PATH,
                WORKER_MAX_DOWNLOAD_ATTEMPTS,
                /* reuseExistingManifestFile= */ true))
        .setRunAttemptCount(0)
        .setWorkerFactory(
            new WorkerFactory() {
              @Override
              public ListenableWorker createWorker(
                  Context appContext, String workerClassName, WorkerParameters workerParameters) {
                return new ManifestDownloadWorker(
                    appContext, workerParameters, DownloaderTestUtils.TestWorker.class);
              }
            })
        .build();
  }
}
