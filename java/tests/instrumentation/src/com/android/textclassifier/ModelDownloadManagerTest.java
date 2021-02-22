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
import android.os.LocaleList;
import android.provider.DeviceConfig.Properties;
import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.work.ExistingWorkPolicy;
import androidx.work.OneTimeWorkRequest;
import androidx.work.WorkInfo;
import androidx.work.WorkManager;
import androidx.work.testing.TestDriver;
import androidx.work.testing.WorkManagerTestInitHelper;
import com.android.textclassifier.ModelFileManager.ModelType;
import com.android.textclassifier.testing.SetDefaultLocalesRule;
import com.google.common.collect.ImmutableList;
import com.google.common.util.concurrent.MoreExecutors;
import java.io.File;
import java.util.HashMap;
import java.util.Locale;
import javax.annotation.Nullable;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public final class ModelDownloadManagerTest {
  private static final String URL_PREFIX = ModelDownloadManager.TEXT_CLASSIFIER_URL_PREFIX;
  private static final String URL_SUFFIX = "abc.xyz";
  private static final String URL_SUFFIX_2 = "def.xyz";
  private static final String URL = URL_PREFIX + URL_SUFFIX;
  private static final String URL_2 = URL_PREFIX + URL_SUFFIX_2;
  // Parameterized test is not yet supported for instrumentation test
  @ModelType.ModelTypeDef private static final String MODEL_TYPE = ModelType.ANNOTATOR;
  @ModelType.ModelTypeDef private static final String MODEL_TYPE_2 = ModelType.ACTIONS_SUGGESTIONS;
  private static final String MODEL_LANGUAGE_TAG = "en";
  private static final String MODEL_LANGUAGE_TAG_2 = "zh";
  private static final String MODEL_LANGUAGE_UNIVERSAL_TAG =
      ModelDownloadManager.UNIVERSAL_MODEL_LANGUAGE_TAG;
  private static final LocaleList DEFAULT_LOCALE_LIST =
      new LocaleList(new Locale(MODEL_LANGUAGE_TAG));

  @Rule public final SetDefaultLocalesRule setDefaultLocalesRule = new SetDefaultLocalesRule();

  // TODO(licha): Maybe we can just use the real TextClassifierSettings
  private FakeDeviceConfig fakeDeviceConfig;
  private WorkManager workManager;
  private TestDriver workManagerTestDriver;
  private File downloadTargetFile;
  private ModelDownloadManager downloadManager;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);
    Context context = ApplicationProvider.getApplicationContext();
    WorkManagerTestInitHelper.initializeTestWorkManager(context);

    this.fakeDeviceConfig = new FakeDeviceConfig();
    this.workManager = WorkManager.getInstance(context);
    this.workManagerTestDriver = WorkManagerTestInitHelper.getTestDriver(context);
    ModelFileManager modelFileManager = new ModelFileManager(context, ImmutableList.of());
    this.downloadTargetFile = modelFileManager.getDownloadTargetFile(MODEL_TYPE, URL_SUFFIX);
    this.downloadManager =
        new ModelDownloadManager(
            workManager,
            DownloaderTestUtils.TestWorker.class,
            modelFileManager,
            new TextClassifierSettings(fakeDeviceConfig),
            MoreExecutors.newDirectExecutorService());
    setDefaultLocalesRule.set(DEFAULT_LOCALE_LIST);
  }

  @After
  public void tearDown() {
    recursiveDelete(ApplicationProvider.getApplicationContext().getFilesDir());
  }

  @Test
  public void init_checkConfigWhenInit() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.init();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
  }

  @Test
  public void checkConfigAndScheduleDownloads_flagNotSet() throws Exception {
    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(workInfo).isNull();
  }

  @Test
  public void checkConfigAndScheduleDownloads_fileAlreadyExists() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    try {
      downloadTargetFile.getParentFile().mkdirs();
      downloadTargetFile.createNewFile();
      downloadManager.checkConfigAndScheduleDownloadsForTesting();

      WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
      assertThat(workInfo).isNull();
    } finally {
      downloadTargetFile.delete();
    }
  }

  @Test
  public void checkConfigAndScheduleDownloads_doNotRedownloadTheSameModel() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    // Simulates a previous model download task
    OneTimeWorkRequest modelDownloadRequest =
        new OneTimeWorkRequest.Builder(DownloaderTestUtils.TestWorker.class).addTag(URL).build();
    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.SUCCESS);
    workManager
        .enqueueUniqueWork(
            ModelDownloadManager.getModelUniqueWorkName(MODEL_TYPE, MODEL_LANGUAGE_TAG),
            ExistingWorkPolicy.REPLACE,
            modelDownloadRequest)
        .getResult()
        .get();

    // Assert the model download work succeeded
    WorkInfo succeededModelWorkInfo =
        DownloaderTestUtils.queryTheOnlyWorkInfo(
            workManager,
            ModelDownloadManager.getModelUniqueWorkName(MODEL_TYPE, MODEL_LANGUAGE_TAG));
    assertThat(succeededModelWorkInfo.getState()).isEqualTo(WorkInfo.State.SUCCEEDED);

    // Trigger the config check
    downloadManager.checkConfigAndScheduleDownloadsForTesting();
    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(workInfo).isNull();
  }

  @Test
  public void checkConfigAndScheduleDownloads_requestEnqueuedSuccessfully() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
  }

  @Test
  public void checkConfigAndScheduleDownloads_multipleModelsEnqueued() throws Exception {
    for (@ModelType.ModelTypeDef String modelType : ModelType.values()) {
      setUpModelUrlSuffix(modelType, MODEL_LANGUAGE_TAG, modelType + URL_SUFFIX);
    }
    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    for (@ModelType.ModelTypeDef String modelType : ModelType.values()) {
      WorkInfo workInfo = queryTheOnlyWorkInfo(modelType, MODEL_LANGUAGE_TAG);
      assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    }
  }

  // This test is to make sure we will not schedule a new task if another task exists with the same
  // url tag, even if it's in a different queue. Currently we schedule both manifest and model
  // download tasks with the same model url tag. This behavior protects us from unintended task
  // overriding.
  @Test
  public void checkConfigAndScheduleDownloads_urlIsCheckedGlobally() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    WorkInfo workInfo1 = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(workInfo1.getState()).isEqualTo(WorkInfo.State.ENQUEUED);

    // Set the same url to a different model type flag
    setUpModelUrlSuffix(MODEL_TYPE_2, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    workInfo1 = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(workInfo1.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    WorkInfo workInfo2 = queryTheOnlyWorkInfo(MODEL_TYPE_2, MODEL_LANGUAGE_TAG);
    assertThat(workInfo2).isNull();
  }

  @Test
  public void checkConfigAndScheduleDownloads_checkMultipleTimes() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();
    WorkInfo oldWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();
    WorkInfo newWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);

    // Will not schedule multiple times, still the same WorkInfo
    assertThat(oldWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(newWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(oldWorkInfo.getId()).isEqualTo(newWorkInfo.getId());
    assertThat(oldWorkInfo.getTags()).containsExactlyElementsIn(newWorkInfo.getTags());
  }

  @Test
  public void checkConfigAndScheduleDownloads_flagUpdatedWhilePrevDownloadPending()
      throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();
    WorkInfo oldWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX_2);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();
    WorkInfo newWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);

    // oldWorkInfo will be replaced with the newWorkInfo
    assertThat(oldWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(newWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(oldWorkInfo.getId()).isNotEqualTo(newWorkInfo.getId());
    assertThat(oldWorkInfo.getTags()).contains(URL);
    assertThat(newWorkInfo.getTags()).contains(URL_2);
  }

  @Test
  public void checkConfigAndScheduleDownloads_flagUpdatedAfterPrevDownloadDone() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();
    WorkInfo oldWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    // Run scheduled download
    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.SUCCESS);
    workManagerTestDriver.setAllConstraintsMet(oldWorkInfo.getId());
    try {
      // Create download file
      downloadTargetFile.createNewFile();
      downloadManager.checkConfigAndScheduleDownloadsForTesting();
      // Update device config
      setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX_2);
      downloadManager.checkConfigAndScheduleDownloadsForTesting();
      WorkInfo newWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);

      // Assert new request can be queued successfully
      assertThat(newWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
      assertThat(newWorkInfo.getTags()).contains(URL_2);
      assertThat(oldWorkInfo.getId()).isNotEqualTo(newWorkInfo.getId());
    } finally {
      downloadTargetFile.delete();
    }
  }

  @Test
  public void checkConfigAndScheduleDownloads_workerSucceeded() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);

    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.SUCCESS);
    workManagerTestDriver.setAllConstraintsMet(workInfo.getId());

    WorkInfo newWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(newWorkInfo.getId()).isEqualTo(workInfo.getId());
    assertThat(newWorkInfo.getState()).isEqualTo(WorkInfo.State.SUCCEEDED);
    assertThat(newWorkInfo.getOutputData().getString(AbstractDownloadWorker.DATA_URL_KEY))
        .isEqualTo(URL);
    assertThat(
            newWorkInfo.getOutputData().getString(AbstractDownloadWorker.DATA_DESTINATION_PATH_KEY))
        .isEqualTo(
            ModelDownloadManager.getTargetManifestPath(downloadTargetFile.getAbsolutePath()));
  }

  @Test
  public void checkConfigAndScheduleDownloads_workerFailed() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);

    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.FAILURE);
    workManagerTestDriver.setAllConstraintsMet(workInfo.getId());

    WorkInfo newWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(newWorkInfo.getId()).isEqualTo(workInfo.getId());
    assertThat(newWorkInfo.getState()).isEqualTo(WorkInfo.State.FAILED);
    assertThat(newWorkInfo.getOutputData().getString(AbstractDownloadWorker.DATA_URL_KEY))
        .isEqualTo(URL);
    assertThat(
            newWorkInfo.getOutputData().getString(AbstractDownloadWorker.DATA_DESTINATION_PATH_KEY))
        .isEqualTo(
            ModelDownloadManager.getTargetManifestPath(downloadTargetFile.getAbsolutePath()));
  }

  @Test
  public void checkConfigAndScheduleDownloads_workerRetried() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG, URL_SUFFIX);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);

    DownloaderTestUtils.TestWorker.setExpectedResult(DownloaderTestUtils.TestWorker.Result.RETRY);
    workManagerTestDriver.setAllConstraintsMet(workInfo.getId());

    WorkInfo newWorkInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG);
    assertThat(newWorkInfo.getId()).isEqualTo(workInfo.getId());
    assertThat(newWorkInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(newWorkInfo.getRunAttemptCount()).isEqualTo(1);
  }

  @Test
  public void checkConfigAndScheduleDownloads_chooseTheBestLocaleTag() throws Exception {
    // System default locale: zh-hant-hk
    setDefaultLocalesRule.set(new LocaleList(Locale.forLanguageTag("zh-hant-hk")));

    // All configured locale tags
    setUpModelUrlSuffix(MODEL_TYPE, "zh-hant", URL_SUFFIX); // best match
    setUpModelUrlSuffix(MODEL_TYPE, "zh", URL_SUFFIX_2); // too general
    setUpModelUrlSuffix(MODEL_TYPE, "zh-hk", URL_SUFFIX_2); // missing script
    setUpModelUrlSuffix(MODEL_TYPE, "zh-hans-hk", URL_SUFFIX_2); // incorrect script
    setUpModelUrlSuffix(MODEL_TYPE, "es-hant-hk", URL_SUFFIX_2); // incorrect language

    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    // The downloader choose: zh-hant
    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "zh-hant").getState())
        .isEqualTo(WorkInfo.State.ENQUEUED);

    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "zh")).isNull();
    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "zh-hk")).isNull();
    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "zh-hans-hk")).isNull();
    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, "es-hant-hk")).isNull();
  }

  @Test
  public void checkConfigAndScheduleDownloads_useUniversalModelIfNoMatchedTag() throws Exception {
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_TAG_2, URL_SUFFIX);
    setUpModelUrlSuffix(MODEL_TYPE, MODEL_LANGUAGE_UNIVERSAL_TAG, URL_SUFFIX_2);
    downloadManager.checkConfigAndScheduleDownloadsForTesting();

    assertThat(queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_TAG_2)).isNull();

    WorkInfo workInfo = queryTheOnlyWorkInfo(MODEL_TYPE, MODEL_LANGUAGE_UNIVERSAL_TAG);
    assertThat(workInfo.getState()).isEqualTo(WorkInfo.State.ENQUEUED);
    assertThat(workInfo.getTags()).contains(URL_2);
  }

  private void setUpModelUrlSuffix(
      @ModelType.ModelTypeDef String modelType, String modelLanguageTag, String urlSuffix) {
    String deviceConfigFlag =
        String.format(
            TextClassifierSettings.MANIFEST_URL_SUFFIX_TEMPLATE, modelType, modelLanguageTag);
    fakeDeviceConfig.setConfig(deviceConfigFlag, urlSuffix);
  }

  /** One unique queue holds at most one request at one time. Returns null if no WorkInfo found. */
  private WorkInfo queryTheOnlyWorkInfo(
      @ModelType.ModelTypeDef String modelType, String modelLanguageTag) throws Exception {
    return DownloaderTestUtils.queryTheOnlyWorkInfo(
        workManager, ModelDownloadManager.getManifestUniqueWorkName(modelType, modelLanguageTag));
  }

  private static void recursiveDelete(File f) {
    if (f.isDirectory()) {
      for (File innerFile : f.listFiles()) {
        recursiveDelete(innerFile);
      }
    }
    f.delete();
  }

  private static class FakeDeviceConfig implements TextClassifierSettings.IDeviceConfig {

    private final HashMap<String, String> configs;

    public FakeDeviceConfig() {
      this.configs = new HashMap<>();
    }

    public void setConfig(String key, String value) {
      configs.put(key, value);
    }

    @Override
    public Properties getProperties(@NonNull String namespace, @NonNull String... names) {
      Properties.Builder builder = new Properties.Builder(namespace);
      for (String key : configs.keySet()) {
        builder.setString(key, configs.get(key));
      }
      return builder.build();
    }

    @Override
    public String getString(
        @NonNull String namespace, @NonNull String name, @Nullable String defaultValue) {
      return configs.containsKey(name) ? configs.get(name) : defaultValue;
    }
  }
}
