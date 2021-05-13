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

import android.content.Context;
import android.os.LocaleList;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.work.ExistingWorkPolicy;
import androidx.work.OneTimeWorkRequest;
import androidx.work.WorkInfo;
import androidx.work.WorkManager;
import androidx.work.testing.TestDriver;
import androidx.work.testing.WorkManagerTestInitHelper;
import com.android.os.AtomsProto.TextClassifierDownloadReported;
import com.android.textclassifier.common.ModelFileManager;
import com.android.textclassifier.common.ModelType;
import com.android.textclassifier.common.TextClassifierSettings;
import com.android.textclassifier.common.statsd.TextClassifierDownloadLoggerTestRule;
import com.android.textclassifier.testing.SetDefaultLocalesRule;
import com.android.textclassifier.testing.TestingDeviceConfig;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.Iterables;
import com.google.common.util.concurrent.MoreExecutors;
import java.util.Locale;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public final class ModelDownloadManagerTest {
  private static final String MANIFEST_URL =
      "https://www.gstatic.com/android/text_classifier/x/v123/en.fb.manifest";
  private static final String MANIFEST_URL_2 =
      "https://www.gstatic.com/android/text_classifier/y/v456/zh.fb.manifest";
  // Parameterized test is not yet supported for instrumentation test
  @ModelType.ModelTypeDef private static final String MODEL_TYPE = ModelType.ANNOTATOR;
  private static final TextClassifierDownloadReported.ModelType MODEL_TYPE_ATOM =
      TextClassifierDownloadReported.ModelType.ANNOTATOR;
  private static final String LOCALE_TAG = "en";
  private static final String LOCALE_TAG_2 = "zh";
  private static final String LOCALE_UNIVERSAL_TAG = ModelDownloadManager.UNIVERSAL_LOCALE_TAG;
  private static final LocaleList DEFAULT_LOCALE_LIST = new LocaleList(new Locale(LOCALE_TAG));

  @Rule public final SetDefaultLocalesRule setDefaultLocalesRule = new SetDefaultLocalesRule();

  @Rule
  public final TextClassifierDownloadLoggerTestRule loggerTestRule =
      new TextClassifierDownloadLoggerTestRule();

  // TODO(licha): Maybe we can just use the real TextClassifierSettings
  private TestingDeviceConfig deviceConfig;
  private WorkManager workManager;
  private TestDriver workManagerTestDriver;
  private ModelDownloadManager downloadManager;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    Context context = ApplicationProvider.getApplicationContext();
    WorkManagerTestInitHelper.initializeTestWorkManager(context);

    this.deviceConfig = new TestingDeviceConfig();
    this.workManager = WorkManager.getInstance(context);
    this.workManagerTestDriver = WorkManagerTestInitHelper.getTestDriver(context);
    ModelFileManager modelFileManager = new ModelFileManager(context, ImmutableList.of());
    this.downloadManager =
        new ModelDownloadManager(
            context,
            DownloaderTestUtils.TestWorker.class,
            modelFileManager,
            new TextClassifierSettings(deviceConfig),
            MoreExecutors.newDirectExecutorService());
    setDefaultLocalesRule.set(DEFAULT_LOCALE_LIST);
    deviceConfig.setConfig(TextClassifierSettings.MODEL_DOWNLOAD_MANAGER_ENABLED, true);
  }

  @After
  public void tearDown() {
    DownloaderTestUtils.deleteRecursively(
        ApplicationProvider.getApplicationContext().getFilesDir());
  }

  @Test
  public void onTextClassifierServiceCreated_requestEnqueued() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    downloadManager.onTextClassifierServiceCreated();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
  }

  @Test
  public void onLocaleChanged_requestEnqueued() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    downloadManager.onLocaleChanged();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_requestEnqueued() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    downloadManager.onTextClassifierDeviceConfigChanged();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_downloaderDisabled() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    deviceConfig.setConfig(TextClassifierSettings.MODEL_DOWNLOAD_MANAGER_ENABLED, false);
    downloadManager.onTextClassifierDeviceConfigChanged();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(workInfo).isNull();
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_flagNotSet() throws Exception {
    downloadManager.onTextClassifierDeviceConfigChanged();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(workInfo).isNull();
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_skipManifestProcessedBefore() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    // Simulates a previous model download task
    OneTimeWorkRequest modelDownloadRequest =
        new OneTimeWorkRequest.Builder(DownloaderTestUtils.TestWorker.class)
            .addTag(MANIFEST_URL)
            .build();
    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.SUCCESS);
    workManager
        .enqueueUniqueWork(
            ModelDownloadManager.formatUniqueWorkName(MODEL_TYPE, LOCALE_TAG),
            ExistingWorkPolicy.REPLACE,
            modelDownloadRequest)
        .getResult()
        .get();

    // Assert the model download work succeeded
    WorkInfo oldWorkInfo =
        DownloaderTestUtils.queryTheOnlyWorkInfo(
            workManager, ModelDownloadManager.formatUniqueWorkName(MODEL_TYPE, LOCALE_TAG));
    assertThat(oldWorkInfo.getState()).isEqualTo(WorkInfo.State.SUCCEEDED);

    // Trigger the config check
    downloadManager.onTextClassifierDeviceConfigChanged();
    WorkInfo newWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(newWorkInfo.getId()).isEqualTo(oldWorkInfo.getId());
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_scheduleWorkForAllModelTypes() throws Exception {
    for (@ModelType.ModelTypeDef String modelType : ModelType.values()) {
      setUpManifestUrl(modelType, LOCALE_TAG, modelType + MANIFEST_URL);
    }
    downloadManager.onTextClassifierDeviceConfigChanged();

    for (@ModelType.ModelTypeDef String modelType : ModelType.values()) {
      WorkInfo workInfo = queryTheOnlyWorkInfo(modelType, LOCALE_TAG);
      assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    }
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_checkIsIdempotent() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    downloadManager.onTextClassifierDeviceConfigChanged();
    WorkInfo oldWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    downloadManager.onTextClassifierDeviceConfigChanged();
    WorkInfo newWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);

    // Will not schedule multiple times, still the same WorkInfo
    assertThat(oldWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(newWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(oldWorkInfo.getId()).isEqualTo(newWorkInfo.getId());
    assertThat(oldWorkInfo.getTags()).containsExactlyElementsIn(newWorkInfo.getTags());
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_newWorkReplaceOldWork() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    downloadManager.onTextClassifierDeviceConfigChanged();
    WorkInfo oldWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL_2);
    downloadManager.onTextClassifierDeviceConfigChanged();
    WorkInfo newWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);

    // oldWorkInfo will be replaced with the newWorkInfo
    assertThat(oldWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(newWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(oldWorkInfo.getId()).isNotEqualTo(newWorkInfo.getId());
    assertThat(oldWorkInfo.getTags()).contains(MANIFEST_URL);
    assertThat(newWorkInfo.getTags()).contains(MANIFEST_URL_2);
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_workerSucceeded() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    downloadManager.onTextClassifierDeviceConfigChanged();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);

    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.SUCCESS);
    workManagerTestDriver.setAllConstraintsMet(workInfo.getId());

    WorkInfo succeededWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(succeededWorkInfo.getId()).isEqualTo(workInfo.getId());
    assertThat(succeededWorkInfo.getState()).isEqualTo(WorkInfo.State.SUCCEEDED);
    assertThat(
            succeededWorkInfo.getOutputData().getString(NewModelDownloadWorker.DATA_MODEL_TYPE_KEY))
        .isEqualTo(MODEL_TYPE);
    assertThat(
            succeededWorkInfo.getOutputData().getString(NewModelDownloadWorker.DATA_LOCALE_TAG_KEY))
        .isEqualTo(LOCALE_TAG);
    assertThat(
            succeededWorkInfo
                .getOutputData()
                .getString(NewModelDownloadWorker.DATA_MANIFEST_URL_KEY))
        .isEqualTo(MANIFEST_URL);
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_workerFailed() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    downloadManager.onTextClassifierDeviceConfigChanged();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);

    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.FAILURE);
    workManagerTestDriver.setAllConstraintsMet(workInfo.getId());

    WorkInfo failedWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(failedWorkInfo.getId()).isEqualTo(workInfo.getId());
    assertThat(failedWorkInfo.getState()).isEqualTo(WorkInfo.State.FAILED);
    assertThat(failedWorkInfo.getOutputData().getString(NewModelDownloadWorker.DATA_MODEL_TYPE_KEY))
        .isEqualTo(MODEL_TYPE);
    assertThat(failedWorkInfo.getOutputData().getString(NewModelDownloadWorker.DATA_LOCALE_TAG_KEY))
        .isEqualTo(LOCALE_TAG);
    assertThat(
            failedWorkInfo.getOutputData().getString(NewModelDownloadWorker.DATA_MANIFEST_URL_KEY))
        .isEqualTo(MANIFEST_URL);
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_workerRetried() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    downloadManager.onTextClassifierDeviceConfigChanged();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);

    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.RETRY);
    workManagerTestDriver.setAllConstraintsMet(workInfo.getId());

    WorkInfo retryWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(retryWorkInfo.getId()).isEqualTo(workInfo.getId());
    assertThat(retryWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(retryWorkInfo.getRunAttemptCount()).isEqualTo(1);
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_chooseTheBestLocaleTag() throws Exception {
    // System default locale: zh-hant-hk
    setDefaultLocalesRule.set(new LocaleList(Locale.forLanguageTag("zh-hant-hk")));

    // All configured locale tags
    setUpManifestUrl(MODEL_TYPE, "zh-hant", MANIFEST_URL); // best match
    setUpManifestUrl(MODEL_TYPE, "zh", MANIFEST_URL_2); // too general
    setUpManifestUrl(MODEL_TYPE, "zh-hk", MANIFEST_URL_2); // missing script
    setUpManifestUrl(MODEL_TYPE, "zh-hans-hk", MANIFEST_URL_2); // incorrect script
    setUpManifestUrl(MODEL_TYPE, "es-hant-hk", MANIFEST_URL_2); // incorrect language

    downloadManager.onTextClassifierDeviceConfigChanged();

    // The downloader choose: zh-hant
    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "zh-hant").getState())
        .isEqualTo(WorkInfo.State.ENQUEUED);

    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "zh")).isNull();
    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "zh-hk")).isNull();
    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "zh-hans-hk")).isNull();
    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "es-hant-hk")).isNull();
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_useUniversalModelIfNoMatchedTag()
      throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG_2, MANIFEST_URL);
    setUpManifestUrl(MODEL_TYPE, LOCALE_UNIVERSAL_TAG, MANIFEST_URL_2);
    downloadManager.onTextClassifierDeviceConfigChanged();

    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG_2)).isNull();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_UNIVERSAL_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(workInfo.getTags()).contains(MANIFEST_URL_2);
  }

  @Test
  public void onTextClassifierDeviceConfigChanged_logAfterDownloadScheduled() throws Exception {
    setUpManifestUrl(MODEL_TYPE, LOCALE_TAG, MANIFEST_URL);
    downloadManager.onTextClassifierDeviceConfigChanged();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, LOCALE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);

    // verify log
    TextClassifierDownloadReported atom = Iterables.getOnlyElement(loggerTestRule.getLoggedAtoms());
    assertThat(atom.getDownloadStatus())
        .isEqualTo(TextClassifierDownloadReported.DownloadStatus.SCHEDULED);
    assertThat(atom.getModelType()).isEqualTo(MODEL_TYPE_ATOM);
    assertThat(atom.getUrlSuffix()).isEqualTo(MANIFEST_URL);
  }

  private void setUpManifestUrl(
      @ModelType.ModelTypeDef String modelType, String localeTag, String url) {
    String deviceConfigFlag =
        String.format(TextClassifierSettings.MANIFEST_URL_TEMPLATE, modelType, localeTag);
    deviceConfig.setConfig(deviceConfigFlag, url);
  }

  /** One unique queue holds at most one request at one time. Returns null if no WorkInfo found. */
  private WorkInfo queryTheOnlyWorkInfo(@ModelType.ModelTypeDef String modelType, String localeTag)
      throws Exception {
    return DownloaderTestUtils.queryTheOnlyWorkInfo(
        workManager, ModelDownloadManager.formatUniqueWorkName(modelType, localeTag));
  }
}
