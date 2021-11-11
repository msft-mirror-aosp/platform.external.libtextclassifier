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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.LocaleList;
import androidx.room.Room;
import androidx.test.core.app.ApplicationProvider;
import androidx.work.ListenableWorker;
import androidx.work.WorkerFactory;
import androidx.work.WorkerParameters;
import androidx.work.testing.TestListenableWorkerBuilder;
import com.android.os.AtomsProto.TextClassifierDownloadReported;
import com.android.os.AtomsProto.TextClassifierDownloadReported.DownloadStatus;
import com.android.os.AtomsProto.TextClassifierDownloadReported.FailureReason;
import com.android.textclassifier.common.ModelType;
import com.android.textclassifier.common.TextClassifierSettings;
import com.android.textclassifier.common.statsd.TextClassifierDownloadLoggerTestRule;
import com.android.textclassifier.testing.SetDefaultLocalesRule;
import com.android.textclassifier.testing.TestingDeviceConfig;
import com.google.common.collect.Iterables;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.MoreExecutors;
import java.io.File;
import java.util.List;
import java.util.Locale;
import java.util.stream.Collectors;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(JUnit4.class)
public final class ModelDownloadWorkerTest {
  private static final String MODEL_TYPE = ModelType.ANNOTATOR;
  private static final String MODEL_TYPE_2 = ModelType.ACTIONS_SUGGESTIONS;
  private static final TextClassifierDownloadReported.ModelType MODEL_TYPE_ATOM =
      TextClassifierDownloadReported.ModelType.ANNOTATOR;
  private static final String LOCALE_TAG = "en";
  private static final String LOCALE_TAG_2 = "zh";
  private static final String LOCALE_TAG_3 = "es";
  private static final String MANIFEST_URL =
      "https://www.gstatic.com/android/text_classifier/q/v711/en.fb.manifest";
  private static final String MANIFEST_URL_2 =
      "https://www.gstatic.com/android/text_classifier/q/v711/zh.fb.manifest";
  private static final String MANIFEST_URL_3 =
      "https://www.gstatic.com/android/text_classifier/q/v711/es.fb.manifest";
  private static final String MODEL_URL =
      "https://www.gstatic.com/android/text_classifier/q/v711/en.fb";
  private static final String MODEL_URL_2 =
      "https://www.gstatic.com/android/text_classifier/q/v711/zh.fb";
  private static final String MODEL_URL_3 =
      "https://www.gstatic.com/android/text_classifier/q/v711/es.fb";
  private static final int RUN_ATTEMPT_COUNT = 1;
  private static final int WORKER_MAX_RUN_ATTEMPT_COUNT = 5;
  private static final int MANIFEST_MAX_ATTEMPT_COUNT = 2;
  private static final ModelManifest.Model MODEL_PROTO =
      ModelManifest.Model.newBuilder()
          .setUrl(MODEL_URL)
          .setSizeInBytes(1)
          .setFingerprint("fingerprint")
          .build();
  private static final ModelManifest.Model MODEL_PROTO_2 =
      ModelManifest.Model.newBuilder()
          .setUrl(MODEL_URL_2)
          .setSizeInBytes(1)
          .setFingerprint("fingerprint")
          .build();
  private static final ModelManifest.Model MODEL_PROTO_3 =
      ModelManifest.Model.newBuilder()
          .setUrl(MODEL_URL_3)
          .setSizeInBytes(1)
          .setFingerprint("fingerprint")
          .build();

  private static final ModelManifest MODEL_MANIFEST_PROTO =
      ModelManifest.newBuilder().addModels(MODEL_PROTO).build();
  private static final ModelManifest MODEL_MANIFEST_PROTO_2 =
      ModelManifest.newBuilder().addModels(MODEL_PROTO_2).build();
  private static final ModelManifest MODEL_MANIFEST_PROTO_3 =
      ModelManifest.newBuilder().addModels(MODEL_PROTO_3).build();
  private static final ModelDownloadException FAILED_TO_DOWNLOAD_EXCEPTION =
      new ModelDownloadException(
          ModelDownloadException.FAILED_TO_DOWNLOAD_OTHER, "failed to download");
  private static final FailureReason FAILED_TO_DOWNLOAD_FAILURE_REASON =
      TextClassifierDownloadReported.FailureReason.FAILED_TO_DOWNLOAD_OTHER;
  private static final LocaleList DEFAULT_LOCALE_LIST = new LocaleList(new Locale(LOCALE_TAG));
  private static final LocaleList LOCALE_LIST_2 =
      new LocaleList(new Locale(LOCALE_TAG), new Locale(LOCALE_TAG_2));
  private static final LocaleList LOCALE_LIST_3 =
      new LocaleList(new Locale(LOCALE_TAG), new Locale(LOCALE_TAG_2), new Locale(LOCALE_TAG_3));

  @Mock private ModelDownloader modelDownloader;
  private File modelDownloaderDir;
  private File modelFile;
  private File modelFile2;
  private File modelFile3;
  private DownloadedModelDatabase db;
  private DownloadedModelManager downloadedModelManager;
  private TestingDeviceConfig deviceConfig;
  private TextClassifierSettings settings;

  @Rule public final SetDefaultLocalesRule setDefaultLocalesRule = new SetDefaultLocalesRule();

  @Rule
  public final TextClassifierDownloadLoggerTestRule loggerTestRule =
      new TextClassifierDownloadLoggerTestRule();

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);

    Context context = ApplicationProvider.getApplicationContext();
    this.deviceConfig = new TestingDeviceConfig();
    this.settings = new TextClassifierSettings(deviceConfig);
    this.modelDownloaderDir = new File(context.getCacheDir(), "downloaded");
    this.modelDownloaderDir.mkdirs();
    this.modelFile = new File(modelDownloaderDir, "test.model");
    this.modelFile2 = new File(modelDownloaderDir, "test2.model");
    this.modelFile3 = new File(modelDownloaderDir, "test3.model");
    this.db = Room.inMemoryDatabaseBuilder(context, DownloadedModelDatabase.class).build();
    this.downloadedModelManager =
        DownloadedModelManagerImpl.getInstanceForTesting(db, modelDownloaderDir, settings);

    setDefaultLocalesRule.set(DEFAULT_LOCALE_LIST);
    deviceConfig.setConfig(TextClassifierSettings.MODEL_DOWNLOAD_MANAGER_ENABLED, true);
  }

  @After
  public void cleanUp() {
    db.close();
    DownloaderTestUtils.deleteRecursively(modelDownloaderDir);
  }

  @Test
  public void downloadSucceed() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    modelFile.createNewFile();
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE)).containsExactly(modelFile);
    verifyLoggedAtom(DownloadStatus.SUCCEEDED, RUN_ATTEMPT_COUNT, /* failureReason= */ null);
  }

  @Test
  public void downloadSucceed_modelAlreadyExists() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    modelFile.createNewFile();
    downloadedModelManager.registerModel(MODEL_URL, modelFile.getAbsolutePath());

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE)).containsExactly(modelFile);
    verifyLoggedAtom(DownloadStatus.SUCCEEDED, RUN_ATTEMPT_COUNT, /* failureReason= */ null);
  }

  @Test
  public void downloadSucceed_manifestAlreadyExists() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    modelFile.createNewFile();
    downloadedModelManager.registerModel(MODEL_URL, modelFile.getAbsolutePath());
    downloadedModelManager.registerManifest(MANIFEST_URL, MODEL_URL);

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE)).containsExactly(modelFile);
    verifyLoggedAtom(DownloadStatus.SUCCEEDED, RUN_ATTEMPT_COUNT, /* failureReason= */ null);
  }

  @Test
  public void downloadSucceed_downloadMultipleModels() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE_2, LOCALE_TAG, MANIFEST_URL_2);
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadManifest(MANIFEST_URL_2))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO_2));
    modelFile.createNewFile();
    modelFile2.createNewFile();
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO_2))
        .thenReturn(Futures.immediateFuture(modelFile2));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(modelFile2.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE)).containsExactly(modelFile);
    assertThat(downloadedModelManager.listModels(MODEL_TYPE_2)).containsExactly(modelFile2);
    List<TextClassifierDownloadReported> atoms = loggerTestRule.getLoggedAtoms();
    assertThat(atoms).hasSize(2);
    assertThat(
            atoms.stream()
                .map(TextClassifierDownloadReported::getUrlSuffix)
                .collect(Collectors.toList()))
        .containsExactly(MANIFEST_URL, MANIFEST_URL_2);
    assertThat(atoms.get(0).getDownloadStatus()).isEqualTo(DownloadStatus.SUCCEEDED);
    assertThat(atoms.get(1).getDownloadStatus()).isEqualTo(DownloadStatus.SUCCEEDED);
  }

  @Test
  public void downloadSucceed_shareSingleModelDownloadForMultipleManifest() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE_2, LOCALE_TAG, MANIFEST_URL_2);
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadManifest(MANIFEST_URL_2))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    modelFile.createNewFile();
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE)).containsExactly(modelFile);
    assertThat(downloadedModelManager.listModels(MODEL_TYPE_2)).containsExactly(modelFile);
    verify(modelDownloader, times(1)).downloadModel(modelDownloaderDir, MODEL_PROTO);
    List<TextClassifierDownloadReported> atoms = loggerTestRule.getLoggedAtoms();
    assertThat(atoms).hasSize(2);
    assertThat(
            atoms.stream()
                .map(TextClassifierDownloadReported::getUrlSuffix)
                .collect(Collectors.toList()))
        .containsExactly(MANIFEST_URL, MANIFEST_URL_2);
    assertThat(atoms.get(0).getDownloadStatus()).isEqualTo(DownloadStatus.SUCCEEDED);
    assertThat(atoms.get(1).getDownloadStatus()).isEqualTo(DownloadStatus.SUCCEEDED);
  }

  @Test
  public void downloadSucceed_shareManifest() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE_2, LOCALE_TAG, MANIFEST_URL);
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    modelFile.createNewFile();
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE)).containsExactly(modelFile);
    assertThat(downloadedModelManager.listModels(MODEL_TYPE_2)).containsExactly(modelFile);
    verify(modelDownloader, times(1)).downloadManifest(MANIFEST_URL);
    List<TextClassifierDownloadReported> atoms = loggerTestRule.getLoggedAtoms();
    assertThat(atoms).hasSize(2);
    assertThat(
            atoms.stream()
                .map(TextClassifierDownloadReported::getUrlSuffix)
                .collect(Collectors.toList()))
        .containsExactly(MANIFEST_URL, MANIFEST_URL);
    assertThat(atoms.get(0).getDownloadStatus()).isEqualTo(DownloadStatus.SUCCEEDED);
    assertThat(atoms.get(1).getDownloadStatus()).isEqualTo(DownloadStatus.SUCCEEDED);
  }

  @Test
  public void downloadFailed_failedToDownloadManifest() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFailedFuture(FAILED_TO_DOWNLOAD_EXCEPTION));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    verifyLoggedAtom(
        DownloadStatus.FAILED_AND_RETRY, RUN_ATTEMPT_COUNT, FAILED_TO_DOWNLOAD_FAILURE_REASON);
  }

  @Test
  public void downloadFailed_failedToDownloadModel() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFailedFuture(FAILED_TO_DOWNLOAD_EXCEPTION));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    verifyLoggedAtom(
        DownloadStatus.FAILED_AND_RETRY, RUN_ATTEMPT_COUNT, FAILED_TO_DOWNLOAD_FAILURE_REASON);
  }

  @Test
  public void downloadFailed_reachWorkerMaxRunAttempts() throws Exception {
    ModelDownloadWorker worker = createWorker(WORKER_MAX_RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.failure());
  }

  @Test
  public void downloadSkipped_reachManifestMaxAttempts() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    deviceConfig.setConfig(
        TextClassifierSettings.MANIFEST_DOWNLOAD_MAX_ATTEMPTS, MANIFEST_MAX_ATTEMPT_COUNT);

    for (int i = 0; i < MANIFEST_MAX_ATTEMPT_COUNT; i++) {
      downloadedModelManager.registerManifestDownloadFailure(MANIFEST_URL);
    }
    ModelDownloadWorker worker = createWorker(MANIFEST_MAX_ATTEMPT_COUNT);

    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(loggerTestRule.getLoggedAtoms()).isEmpty();
  }

  @Test
  public void downloadSkipped_manifestAlreadyProcessed() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    modelFile.createNewFile();
    downloadedModelManager.registerModel(MODEL_URL, modelFile.getAbsolutePath());
    downloadedModelManager.registerManifest(MANIFEST_URL, MODEL_URL);
    downloadedModelManager.registerManifestEnrollment(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);

    ModelDownloadWorker worker = createWorker(MANIFEST_MAX_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(loggerTestRule.getLoggedAtoms()).isEmpty();
  }

  @Test
  public void downloadSucceeded_multiLanguageSupportEnabled() throws Exception {
    setDefaultLocalesRule.set(LOCALE_LIST_2);
    deviceConfig.setConfig(TextClassifierSettings.MULTI_LANGUAGE_SUPPORT_ENABLED, true);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG_2, MANIFEST_URL_2);

    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadManifest(MANIFEST_URL_2))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO_2));

    modelFile.createNewFile();
    modelFile2.createNewFile();
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO_2))
        .thenReturn(Futures.immediateFuture(modelFile2));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(modelFile2.exists()).isTrue();

    assertThat(downloadedModelManager.listModels(MODEL_TYPE))
        .containsExactly(modelFile, modelFile2);
  }

  @Test
  public void downloadSucceeded_multiLanguageSupportEnabled_singleLocale() throws Exception {
    setDefaultLocalesRule.set(DEFAULT_LOCALE_LIST);
    deviceConfig.setConfig(TextClassifierSettings.MULTI_LANGUAGE_SUPPORT_ENABLED, true);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);

    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));

    modelFile.createNewFile();
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE)).containsExactly(modelFile);
  }

  @Test
  public void downloadSucceeded_multiLanguageSupportEnabled_oneManifestAlreadyDownloaded()
      throws Exception {
    setDefaultLocalesRule.set(LOCALE_LIST_2);
    deviceConfig.setConfig(TextClassifierSettings.MULTI_LANGUAGE_SUPPORT_ENABLED, true);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG_2, MANIFEST_URL_2);

    modelFile.createNewFile();
    downloadedModelManager.registerModel(MODEL_URL, modelFile.getAbsolutePath());
    downloadedModelManager.registerManifest(MANIFEST_URL, MODEL_URL);
    downloadedModelManager.registerManifestEnrollment(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);

    when(modelDownloader.downloadManifest(MANIFEST_URL_2))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO_2));
    modelFile2.createNewFile();
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO_2))
        .thenReturn(Futures.immediateFuture(modelFile2));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(modelFile2.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE))
        .containsExactly(modelFile, modelFile2);
  }

  @Test
  public void downloadSucceeded_multiLanguageSupportDisabled() throws Exception {
    setDefaultLocalesRule.set(LOCALE_LIST_2);
    deviceConfig.setConfig(TextClassifierSettings.MULTI_LANGUAGE_SUPPORT_ENABLED, false);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG_2, MANIFEST_URL_2);

    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadManifest(MANIFEST_URL_2))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO_2));

    modelFile.createNewFile();
    modelFile2.createNewFile();
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO_2))
        .thenReturn(Futures.immediateFuture(modelFile2));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE)).containsExactly(modelFile);
  }

  @Test
  public void oneDownloadFailed_multiLanguageSupportEnabled() throws Exception {
    setDefaultLocalesRule.set(LOCALE_LIST_2);
    deviceConfig.setConfig(TextClassifierSettings.MULTI_LANGUAGE_SUPPORT_ENABLED, true);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG_2, MANIFEST_URL_2);

    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadManifest(MANIFEST_URL_2))
        .thenReturn(Futures.immediateFailedFuture(FAILED_TO_DOWNLOAD_EXCEPTION));

    modelFile.createNewFile();
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    assertThat(modelFile.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE)).containsExactly(modelFile);
  }

  @Test
  public void downloadSucceeded_multiLanguageSupportEnabled_checkLimit() throws Exception {
    setDefaultLocalesRule.set(LOCALE_LIST_3);
    deviceConfig.setConfig(TextClassifierSettings.MULTI_LANGUAGE_SUPPORT_ENABLED, true);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG_2, MANIFEST_URL_2);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG_3, MANIFEST_URL_3);

    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadManifest(MANIFEST_URL_2))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO_2));
    when(modelDownloader.downloadManifest(MANIFEST_URL_3))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO_3));

    modelFile.createNewFile();
    modelFile2.createNewFile();
    modelFile3.createNewFile();

    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO_2))
        .thenReturn(Futures.immediateFuture(modelFile2));
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO_3))
        .thenReturn(Futures.immediateFuture(modelFile3));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(modelFile2.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE))
        .containsExactly(modelFile, modelFile2);
  }

  @Test
  public void downloadSucceed_multiLanguageSupportEnabled_checkDownloadedModelTypes()
      throws Exception {
    setDefaultLocalesRule.set(LOCALE_LIST_2);
    deviceConfig.setConfig(TextClassifierSettings.MULTI_LANGUAGE_SUPPORT_ENABLED, true);
    deviceConfig.setConfig(
        TextClassifierSettings.ENABLED_MODEL_TYPES_FOR_MULTI_LANGUAGE_SUPPORT, MODEL_TYPE);
    setUpManifestUrl(MODEL_TYPE_2, LOCALE_TAG, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE_2, LOCALE_TAG_2, MANIFEST_URL_2);

    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadManifest(MANIFEST_URL_2))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO_2));

    modelFile.createNewFile();
    modelFile2.createNewFile();

    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(modelFile));
    when(modelDownloader.downloadModel(modelDownloaderDir, MODEL_PROTO_2))
        .thenReturn(Futures.immediateFuture(modelFile2));

    ModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(modelFile.exists()).isTrue();
    assertThat(downloadedModelManager.listModels(MODEL_TYPE_2)).containsExactly(modelFile);
  }

  private ModelDownloadWorker createWorker(int runAttemptCount) {
    return TestListenableWorkerBuilder.from(
            ApplicationProvider.getApplicationContext(), ModelDownloadWorker.class)
        .setRunAttemptCount(runAttemptCount)
        .setWorkerFactory(
            new WorkerFactory() {
              @Override
              public ListenableWorker createWorker(
                  Context appContext, String workerClassName, WorkerParameters workerParameters) {
                return new ModelDownloadWorker(
                    appContext,
                    workerParameters,
                    MoreExecutors.newDirectExecutorService(),
                    modelDownloader,
                    downloadedModelManager,
                    settings);
              }
            })
        .build();
  }

  private void verifyLoggedAtom(
      DownloadStatus downloadStatus, long runAttemptCount, FailureReason failureReason)
      throws Exception {
    TextClassifierDownloadReported atom = Iterables.getOnlyElement(loggerTestRule.getLoggedAtoms());
    assertThat(atom.getDownloadStatus()).isEqualTo(downloadStatus);
    assertThat(atom.getModelType()).isEqualTo(MODEL_TYPE_ATOM);
    assertThat(atom.getUrlSuffix()).isEqualTo(MANIFEST_URL);
    assertThat(atom.getRunAttemptCount()).isEqualTo(runAttemptCount);
    if (failureReason != null) {
      assertThat(atom.getFailureReason()).isEqualTo(failureReason);
    }
  }

  private void setUpManifestUrl(
      @ModelType.ModelTypeDef String modelType, String localeTag, String url) {
    String deviceConfigFlag =
        String.format(TextClassifierSettings.MANIFEST_URL_TEMPLATE, modelType, localeTag);
    deviceConfig.setConfig(deviceConfigFlag, url);
  }
}
