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

import static com.android.textclassifier.ModelFileManager.ModelFile.LANGUAGE_INDEPENDENT;
import static com.google.common.truth.Truth.assertThat;

import android.os.LocaleList;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.android.textclassifier.ModelFileManager.ModelFile;
import com.android.textclassifier.ModelFileManager.RegularFileFullMatchLister;
import com.android.textclassifier.ModelFileManager.RegularFilePatternMatchLister;
import com.android.textclassifier.common.ModelType;
import com.android.textclassifier.common.ModelType.ModelTypeDef;
import com.android.textclassifier.common.TextClassifierSettings;
import com.android.textclassifier.common.logging.ResultIdUtils.ModelInfo;
import com.android.textclassifier.testing.SetDefaultLocalesRule;
import com.google.common.base.Optional;
import com.google.common.collect.ImmutableList;
import com.google.common.io.Files;
import java.io.File;
import java.io.IOException;
import java.util.List;
import java.util.Locale;
import java.util.stream.Collectors;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@SmallTest
@RunWith(AndroidJUnit4.class)
public final class ModelFileManagerTest {
  private static final Locale DEFAULT_LOCALE = Locale.forLanguageTag("en-US");

  @ModelTypeDef private static final String MODEL_TYPE = ModelType.ANNOTATOR;

  @Mock private TextClassifierSettings.IDeviceConfig mockDeviceConfig;

  @Rule public final SetDefaultLocalesRule setDefaultLocalesRule = new SetDefaultLocalesRule();

  private File rootTestDir;
  private ModelFileManager modelFileManager;

  @Before
  public void setup() {
    MockitoAnnotations.initMocks(this);

    rootTestDir =
        new File(ApplicationProvider.getApplicationContext().getCacheDir(), "rootTestDir");
    rootTestDir.mkdirs();
    modelFileManager =
        new ModelFileManager(
            ApplicationProvider.getApplicationContext(),
            new TextClassifierSettings(mockDeviceConfig));
    setDefaultLocalesRule.set(new LocaleList(DEFAULT_LOCALE));
  }

  @After
  public void removeTestDir() {
    recursiveDelete(rootTestDir);
  }

  @Test
  public void annotatorModelPreloaded() {
    verifyModelPreloadedAsAsset(ModelType.ANNOTATOR, "textclassifier/annotator.universal.model");
  }

  @Test
  public void actionsModelPreloaded() {
    verifyModelPreloadedAsAsset(
        ModelType.ACTIONS_SUGGESTIONS, "textclassifier/actions_suggestions.universal.model");
  }

  @Test
  public void langIdModelPreloaded() {
    verifyModelPreloadedAsAsset(ModelType.LANG_ID, "textclassifier/lang_id.model");
  }

  private void verifyModelPreloadedAsAsset(
      @ModelTypeDef String modelType, String expectedModelPath) {
    List<ModelFileManager.ModelFile> modelFiles = modelFileManager.listModelFiles(modelType);
    List<ModelFile> assetFiles =
        modelFiles.stream().filter(modelFile -> modelFile.isAsset).collect(Collectors.toList());

    assertThat(assetFiles).hasSize(1);
    assertThat(assetFiles.get(0).absolutePath).isEqualTo(expectedModelPath);
  }

  @Test
  public void findBestModel_versionCode() {
    ModelFileManager.ModelFile olderModelFile =
        createModelFile(LANGUAGE_INDEPENDENT, /* version */ 1);
    ModelFileManager.ModelFile newerModelFile =
        createModelFile(LANGUAGE_INDEPENDENT, /* version */ 2);
    ModelFileManager modelFileManager = createModelFileManager(olderModelFile, newerModelFile);

    ModelFile bestModelFile = modelFileManager.findBestModelFile(MODEL_TYPE, null);
    assertThat(bestModelFile).isEqualTo(newerModelFile);
  }

  @Test
  public void findBestModel_languageDependentModelIsPreferred() {
    ModelFileManager.ModelFile languageIndependentModelFile =
        createModelFile(LANGUAGE_INDEPENDENT, /* version */ 1);
    ModelFileManager.ModelFile languageDependentModelFile =
        createModelFile(DEFAULT_LOCALE.toLanguageTag(), /* version */ 1);
    ModelFileManager modelFileManager =
        createModelFileManager(languageIndependentModelFile, languageDependentModelFile);

    ModelFile bestModelFile =
        modelFileManager.findBestModelFile(MODEL_TYPE, new LocaleList(DEFAULT_LOCALE));
    assertThat(bestModelFile).isEqualTo(languageDependentModelFile);
  }

  @Test
  public void findBestModel_noMatchedLanguageModel() {
    ModelFileManager.ModelFile languageIndependentModelFile =
        createModelFile(LANGUAGE_INDEPENDENT, /* version */ 1);
    ModelFileManager.ModelFile languageDependentModelFile =
        createModelFile("zh-hk", /* version */ 1);
    ModelFileManager modelFileManager =
        createModelFileManager(languageIndependentModelFile, languageDependentModelFile);

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(MODEL_TYPE, new LocaleList(DEFAULT_LOCALE));
    assertThat(bestModelFile).isEqualTo(languageIndependentModelFile);
  }

  @Test
  public void findBestModel_languageIsMoreImportantThanVersion() {
    ModelFileManager.ModelFile matchButOlderModel =
        createModelFile(DEFAULT_LOCALE.toLanguageTag(), /* version */ 1);
    ModelFileManager.ModelFile mismatchButNewerModel = createModelFile("zh-hk", /* version */ 2);
    ModelFileManager modelFileManager =
        createModelFileManager(matchButOlderModel, mismatchButNewerModel);

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(MODEL_TYPE, new LocaleList(DEFAULT_LOCALE));
    assertThat(bestModelFile).isEqualTo(matchButOlderModel);
  }

  @Test
  public void findBestModel_filterOutLocalePreferenceNotInDefaultLocaleList_onlyCheckLanguage() {
    setDefaultLocalesRule.set(LocaleList.forLanguageTags("zh"));
    ModelFileManager.ModelFile languageIndependentModelFile =
        createModelFile(LANGUAGE_INDEPENDENT, /* version */ 1);
    ModelFileManager.ModelFile languageDependentModelFile = createModelFile("zh", /* version */ 1);
    ModelFileManager modelFileManager =
        createModelFileManager(languageIndependentModelFile, languageDependentModelFile);

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(MODEL_TYPE, LocaleList.forLanguageTags("zh-hk"));
    assertThat(bestModelFile).isEqualTo(languageDependentModelFile);
  }

  @Test
  public void findBestModel_filterOutLocalePreferenceNotInDefaultLocaleList_match() {
    setDefaultLocalesRule.set(LocaleList.forLanguageTags("zh-hk"));
    ModelFileManager.ModelFile languageIndependentModelFile =
        createModelFile(LANGUAGE_INDEPENDENT, /* version */ 1);
    ModelFileManager.ModelFile languageDependentModelFile = createModelFile("zh", /* version */ 1);
    ModelFileManager modelFileManager =
        createModelFileManager(languageIndependentModelFile, languageDependentModelFile);

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(MODEL_TYPE, LocaleList.forLanguageTags("zh"));
    assertThat(bestModelFile).isEqualTo(languageDependentModelFile);
  }

  @Test
  public void findBestModel_filterOutLocalePreferenceNotInDefaultLocaleList_doNotMatch() {
    setDefaultLocalesRule.set(LocaleList.forLanguageTags("en"));
    ModelFileManager.ModelFile languageIndependentModelFile =
        createModelFile(LANGUAGE_INDEPENDENT, /* version */ 1);
    ModelFileManager.ModelFile languageDependentModelFile = createModelFile("zh", /* version */ 1);
    ModelFileManager modelFileManager =
        createModelFileManager(languageIndependentModelFile, languageDependentModelFile);

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(MODEL_TYPE, LocaleList.forLanguageTags("zh"));
    assertThat(bestModelFile).isEqualTo(languageIndependentModelFile);
  }

  @Test
  public void findBestModel_onlyPrimaryLocaleConsidered_noLocalePreferencesProvided() {
    setDefaultLocalesRule.set(
        new LocaleList(Locale.forLanguageTag("en"), Locale.forLanguageTag("zh-hk")));
    ModelFileManager.ModelFile languageIndependentModelFile =
        createModelFile(LANGUAGE_INDEPENDENT, /* version */ 1);
    ModelFileManager.ModelFile nonPrimaryLocaleModelFile =
        createModelFile("zh-hk", /* version */ 1);
    ModelFileManager modelFileManager =
        createModelFileManager(languageIndependentModelFile, nonPrimaryLocaleModelFile);

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(MODEL_TYPE, /* localePreferences= */ null);
    assertThat(bestModelFile).isEqualTo(languageIndependentModelFile);
  }

  @Test
  public void findBestModel_onlyPrimaryLocaleConsidered_localePreferencesProvided() {
    setDefaultLocalesRule.set(
        new LocaleList(Locale.forLanguageTag("en"), Locale.forLanguageTag("zh-hk")));

    ModelFileManager.ModelFile languageIndependentModelFile =
        createModelFile(LANGUAGE_INDEPENDENT, /* version */ 1);
    ModelFileManager.ModelFile nonPrimaryLocalePreferenceModelFile =
        createModelFile("zh-hk", /* version */ 1);
    ModelFileManager modelFileManager =
        createModelFileManager(languageIndependentModelFile, nonPrimaryLocalePreferenceModelFile);

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(
            MODEL_TYPE,
            new LocaleList(Locale.forLanguageTag("en"), Locale.forLanguageTag("zh-hk")));
    assertThat(bestModelFile).isEqualTo(languageIndependentModelFile);
  }

  @Test
  public void modelFileEquals() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/a", /* version= */ 1, "ja", /* isAsset= */ false);

    ModelFileManager.ModelFile modelB =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/a", /* version= */ 1, "ja", /* isAsset= */ false);

    assertThat(modelA).isEqualTo(modelB);
  }

  @Test
  public void modelFile_different() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/a", /* version= */ 1, "ja", /* isAsset= */ false);
    ModelFileManager.ModelFile modelB =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/b", /* version= */ 1, "ja", /* isAsset= */ false);

    assertThat(modelA).isNotEqualTo(modelB);
  }

  @Test
  public void modelFile_isPreferredTo_languageDependentIsBetter() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/a", /* version= */ 1, "ja", /* isAsset= */ false);

    ModelFileManager.ModelFile modelB =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/b", /* version= */ 2, LANGUAGE_INDEPENDENT, /* isAsset= */ false);

    assertThat(modelA.isPreferredTo(modelB)).isTrue();
  }

  @Test
  public void modelFile_isPreferredTo_version() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/a", /* version= */ 2, "ja", /* isAsset= */ false);

    ModelFileManager.ModelFile modelB =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/b", /* version= */ 1, "ja", /* isAsset= */ false);

    assertThat(modelA.isPreferredTo(modelB)).isTrue();
  }

  @Test
  public void modelFile_toModelInfo() {
    ModelFileManager.ModelFile modelFile =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/a", /* version= */ 2, "ja", /* isAsset= */ false);

    ModelInfo modelInfo = modelFile.toModelInfo();

    assertThat(modelInfo.toModelName()).isEqualTo("ja_v2");
  }

  @Test
  public void modelFile_toModelInfo_universal() {
    ModelFileManager.ModelFile modelFile =
        new ModelFileManager.ModelFile(
            MODEL_TYPE, "/path/a", /* version= */ 2, "*", /* isAsset= */ false);

    ModelInfo modelInfo = modelFile.toModelInfo();

    assertThat(modelInfo.toModelName()).isEqualTo("*_v2");
  }

  @Test
  public void modelFile_toModelInfos() {
    ModelFile englishModelFile =
        new ModelFile(MODEL_TYPE, "/path/a", /* version= */ 1, "en", /* isAsset= */ false);
    ModelFile japaneseModelFile =
        new ModelFile(MODEL_TYPE, "/path/a", /* version= */ 2, "ja", /* isAsset= */ false);

    ImmutableList<Optional<ModelInfo>> modelInfos =
        ModelFileManager.ModelFile.toModelInfos(
            Optional.of(englishModelFile), Optional.of(japaneseModelFile));

    assertThat(
            modelInfos.stream()
                .map(modelFile -> modelFile.transform(ModelInfo::toModelName).or(""))
                .collect(Collectors.toList()))
        .containsExactly("en_v1", "ja_v2")
        .inOrder();
  }

  @Test
  public void regularFileFullMatchLister() throws IOException {
    File modelFile = new File(rootTestDir, "test.model");
    Files.copy(TestDataUtils.getTestAnnotatorModelFile(), modelFile);
    File wrongFile = new File(rootTestDir, "wrong.model");
    Files.copy(TestDataUtils.getTestAnnotatorModelFile(), wrongFile);

    RegularFileFullMatchLister regularFileFullMatchLister =
        new RegularFileFullMatchLister(MODEL_TYPE, modelFile, () -> true);
    ImmutableList<ModelFile> listedModels = regularFileFullMatchLister.list(MODEL_TYPE);

    assertThat(listedModels).hasSize(1);
    assertThat(listedModels.get(0).absolutePath).isEqualTo(modelFile.getAbsolutePath());
    assertThat(listedModels.get(0).isAsset).isFalse();
  }

  @Test
  public void regularFilePatternMatchLister() throws IOException {
    File modelFile1 = new File(rootTestDir, "annotator.en.model");
    Files.copy(TestDataUtils.getTestAnnotatorModelFile(), modelFile1);
    File modelFile2 = new File(rootTestDir, "annotator.fr.model");
    Files.copy(TestDataUtils.getTestAnnotatorModelFile(), modelFile2);
    File mismatchedModelFile = new File(rootTestDir, "actions.en.model");
    Files.copy(TestDataUtils.getTestAnnotatorModelFile(), mismatchedModelFile);

    RegularFilePatternMatchLister regularFilePatternMatchLister =
        new RegularFilePatternMatchLister(
            MODEL_TYPE, rootTestDir, "annotator\\.(.*)\\.model", () -> true);
    ImmutableList<ModelFile> listedModels = regularFilePatternMatchLister.list(MODEL_TYPE);

    assertThat(listedModels).hasSize(2);
    assertThat(listedModels.get(0).isAsset).isFalse();
    assertThat(listedModels.get(1).isAsset).isFalse();
    assertThat(ImmutableList.of(listedModels.get(0).absolutePath, listedModels.get(1).absolutePath))
        .containsExactly(modelFile1.getAbsolutePath(), modelFile2.getAbsolutePath());
  }

  @Test
  public void regularFilePatternMatchLister_disabled() throws IOException {
    File modelFile1 = new File(rootTestDir, "annotator.en.model");
    Files.copy(TestDataUtils.getTestAnnotatorModelFile(), modelFile1);

    RegularFilePatternMatchLister regularFilePatternMatchLister =
        new RegularFilePatternMatchLister(
            MODEL_TYPE, rootTestDir, "annotator\\.(.*)\\.model", () -> false);
    ImmutableList<ModelFile> listedModels = regularFilePatternMatchLister.list(MODEL_TYPE);

    assertThat(listedModels).isEmpty();
  }

  private ModelFileManager createModelFileManager(ModelFile... modelFiles) {
    return new ModelFileManager(
        ApplicationProvider.getApplicationContext(),
        ImmutableList.of(modelType -> ImmutableList.copyOf(modelFiles)));
  }

  private ModelFileManager.ModelFile createModelFile(String supportedLocaleTags, int version) {
    return new ModelFileManager.ModelFile(
        MODEL_TYPE,
        new File(rootTestDir, String.format("%s-%d", supportedLocaleTags, version))
            .getAbsolutePath(),
        version,
        supportedLocaleTags,
        /* isAsset= */ false);
  }

  private static void recursiveDelete(File f) {
    if (f.isDirectory()) {
      for (File innerFile : f.listFiles()) {
        recursiveDelete(innerFile);
      }
    }
    f.delete();
  }
}
