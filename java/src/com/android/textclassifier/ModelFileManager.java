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
import android.os.ParcelFileDescriptor;
import android.text.TextUtils;
import androidx.annotation.GuardedBy;
import androidx.annotation.StringDef;
import com.android.textclassifier.ModelFileManager.ModelFile;
import com.android.textclassifier.ModelFileManager.ModelFile.ModelType;
import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.common.logging.ResultIdUtils.ModelInfo;
import com.android.textclassifier.utils.IndentingPrintWriter;
import com.google.android.textclassifier.ActionsSuggestionsModel;
import com.google.android.textclassifier.AnnotatorModel;
import com.google.android.textclassifier.LangIdModel;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.function.Function;
import java.util.function.Supplier;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import javax.annotation.Nullable;

/**
 * Manages all model files in storage. {@link TextClassifierImpl} depends on this class to get the
 * model files to load.
 */
final class ModelFileManager {
  private static final String TAG = "ModelFileManager";

  private final ImmutableMap<String, Supplier<ImmutableList<ModelFile>>> modelFileSuppliers;

  /** Create a ModelFileManager based on hardcoded model file locations. */
  public static ModelFileManager create(TextClassifierSettings settings) {
    ImmutableMap.Builder<String, Supplier<ImmutableList<ModelFile>>> suppliersBuilder =
        ImmutableMap.builder();
    for (String modelType : ModelType.values()) {
      suppliersBuilder.put(modelType, new ModelFileSupplierImpl(settings, modelType));
    }
    return new ModelFileManager(suppliersBuilder.build());
  }

  @VisibleForTesting
  ModelFileManager(ImmutableMap<String, Supplier<ImmutableList<ModelFile>>> modelFileSuppliers) {
    this.modelFileSuppliers = modelFileSuppliers;
  }

  /**
   * Returns an immutable list of model files listed by the given model files supplier.
   *
   * @param modelType which type of model files to look for
   */
  public ImmutableList<ModelFile> listModelFiles(@ModelType.ModelTypeDef String modelType) {
    if (modelFileSuppliers.containsKey(modelType)) {
      return modelFileSuppliers.get(modelType).get();
    }
    return ImmutableList.of();
  }

  /**
   * Returns the best model file for the given localelist, {@code null} if nothing is found.
   *
   * @param modelType the type of model to look up (e.g. annotator, lang_id, etc.)
   * @param localeList an ordered list of user preferences for locales, use {@code null} if there is
   *     no preference.
   */
  @Nullable
  public ModelFile findBestModelFile(
      @ModelType.ModelTypeDef String modelType, @Nullable LocaleList localeList) {
    final String languages =
        localeList == null || localeList.isEmpty()
            ? LocaleList.getDefault().toLanguageTags()
            : localeList.toLanguageTags();
    final List<Locale.LanguageRange> languageRangeList = Locale.LanguageRange.parse(languages);

    ModelFile bestModel = null;
    for (ModelFile model : listModelFiles(modelType)) {
      if (model.isAnyLanguageSupported(languageRangeList)) {
        if (model.isPreferredTo(bestModel)) {
          bestModel = model;
        }
      }
    }
    return bestModel;
  }

  /**
   * Dumps the internal state for debugging.
   *
   * @param printWriter writer to write dumped states
   */
  public void dump(IndentingPrintWriter printWriter) {
    printWriter.println("ModelFileManager:");
    printWriter.increaseIndent();
    for (@ModelType.ModelTypeDef String modelType : ModelType.values()) {
      printWriter.println(modelType + " model file(s):");
      printWriter.increaseIndent();
      for (ModelFile modelFile : listModelFiles(modelType)) {
        printWriter.println(modelFile.toString());
      }
      printWriter.decreaseIndent();
    }
    printWriter.decreaseIndent();
  }

  /** Default implementation of the model file supplier. */
  @VisibleForTesting
  static final class ModelFileSupplierImpl implements Supplier<ImmutableList<ModelFile>> {
    private static final String FACTORY_MODEL_DIR = "/etc/textclassifier/";

    private static final class ModelFileInfo {
      private final String factoryModelNameRegex;
      private final String configUpdaterModelPath;
      private final Function<Integer, Integer> versionSupplier;
      private final Function<Integer, String> supportedLocalesSupplier;

      public ModelFileInfo(
          String factoryModelNameRegex,
          String configUpdaterModelPath,
          Function<Integer, Integer> versionSupplier,
          Function<Integer, String> supportedLocalesSupplier) {
        this.factoryModelNameRegex = factoryModelNameRegex;
        this.configUpdaterModelPath = configUpdaterModelPath;
        this.versionSupplier = versionSupplier;
        this.supportedLocalesSupplier = supportedLocalesSupplier;
      }

      public String getFactoryModelNameRegex() {
        return factoryModelNameRegex;
      }

      public String getConfigUpdaterModelPath() {
        return configUpdaterModelPath;
      }

      public Function<Integer, Integer> getVersionSupplier() {
        return versionSupplier;
      }

      public Function<Integer, String> getSupportedLocalesSupplier() {
        return supportedLocalesSupplier;
      }
    }

    private static final ImmutableMap<String, ModelFileInfo> MODEL_FILE_INFO_MAP =
        ImmutableMap.<String, ModelFileInfo>builder()
            .put(
                ModelType.ANNOTATOR,
                new ModelFileInfo(
                    "textclassifier\\.(.*)\\.model",
                    "/data/misc/textclassifier/textclassifier.model",
                    AnnotatorModel::getVersion,
                    AnnotatorModel::getLocales))
            .put(
                ModelType.LANG_ID,
                new ModelFileInfo(
                    "lang_id.model",
                    "/data/misc/textclassifier/lang_id.model",
                    LangIdModel::getVersion,
                    fd -> ModelFile.LANGUAGE_INDEPENDENT))
            .put(
                ModelType.ACTIONS_SUGGESTIONS,
                new ModelFileInfo(
                    "actions_suggestions\\.(.*)\\.model",
                    "/data/misc/textclassifier/actions_suggestions.model",
                    ActionsSuggestionsModel::getVersion,
                    ActionsSuggestionsModel::getLocales))
            .build();

    private final TextClassifierSettings settings;
    @ModelType.ModelTypeDef private final String modelType;
    private final File configUpdaterModelFile;
    private final File downloaderModelFile;
    private final File factoryModelDir;
    private final Pattern modelFilenamePattern;
    private final Function<Integer, Integer> versionSupplier;
    private final Function<Integer, String> supportedLocalesSupplier;
    private final Object lock = new Object();

    @GuardedBy("lock")
    private ImmutableList<ModelFile> factoryModels;

    public ModelFileSupplierImpl(
        TextClassifierSettings settings, @ModelType.ModelTypeDef String modelType) {
      this(
          settings,
          modelType,
          new File(FACTORY_MODEL_DIR),
          MODEL_FILE_INFO_MAP.get(modelType).getFactoryModelNameRegex(),
          new File(MODEL_FILE_INFO_MAP.get(modelType).getConfigUpdaterModelPath()),
          /* downloaderModelFile= */ null,
          MODEL_FILE_INFO_MAP.get(modelType).getVersionSupplier(),
          MODEL_FILE_INFO_MAP.get(modelType).getSupportedLocalesSupplier());
    }

    @VisibleForTesting
    ModelFileSupplierImpl(
        TextClassifierSettings settings,
        @ModelType.ModelTypeDef String modelType,
        File factoryModelDir,
        String factoryModelFileNameRegex,
        File configUpdaterModelFile,
        @Nullable File downloaderModelFile,
        Function<Integer, Integer> versionSupplier,
        Function<Integer, String> supportedLocalesSupplier) {
      this.settings = settings;
      this.modelType = modelType;
      this.factoryModelDir = Preconditions.checkNotNull(factoryModelDir);
      this.modelFilenamePattern =
          Pattern.compile(Preconditions.checkNotNull(factoryModelFileNameRegex));
      this.configUpdaterModelFile = Preconditions.checkNotNull(configUpdaterModelFile);
      this.downloaderModelFile = downloaderModelFile;
      this.versionSupplier = Preconditions.checkNotNull(versionSupplier);
      this.supportedLocalesSupplier = Preconditions.checkNotNull(supportedLocalesSupplier);
    }

    @Override
    public ImmutableList<ModelFile> get() {
      final List<ModelFile> modelFiles = new ArrayList<>();
      // The dwonloader and config updater model have higher precedences.
      if (downloaderModelFile != null
          && downloaderModelFile.exists()
          && settings.getModelDownloadManagerEnabled()) {
        final ModelFile downloaderModel = createModelFile(downloaderModelFile);
        if (downloaderModel != null) {
          modelFiles.add(downloaderModel);
        }
      }
      if (configUpdaterModelFile.exists()) {
        final ModelFile updatedModel = createModelFile(configUpdaterModelFile);
        if (updatedModel != null) {
          modelFiles.add(updatedModel);
        }
      }
      // Factory models should never have overlapping locales, so the order doesn't matter.
      synchronized (lock) {
        if (factoryModels == null) {
          factoryModels = getFactoryModels();
        }
        modelFiles.addAll(factoryModels);
      }
      return ImmutableList.copyOf(modelFiles);
    }

    private ImmutableList<ModelFile> getFactoryModels() {
      List<ModelFile> factoryModelFiles = new ArrayList<>();
      if (factoryModelDir.exists() && factoryModelDir.isDirectory()) {
        final File[] files = factoryModelDir.listFiles();
        for (File file : files) {
          final Matcher matcher = modelFilenamePattern.matcher(file.getName());
          if (matcher.matches() && file.isFile()) {
            final ModelFile model = createModelFile(file);
            if (model != null) {
              factoryModelFiles.add(model);
            }
          }
        }
      }
      return ImmutableList.copyOf(factoryModelFiles);
    }

    /** Returns null if the path did not point to a compatible model. */
    @Nullable
    private ModelFile createModelFile(File file) {
      if (!file.exists()) {
        return null;
      }
      ParcelFileDescriptor modelFd = null;
      try {
        modelFd = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
        if (modelFd == null) {
          return null;
        }
        final int modelFdInt = modelFd.getFd();
        final int version = versionSupplier.apply(modelFdInt);
        final String supportedLocalesStr = supportedLocalesSupplier.apply(modelFdInt);
        if (supportedLocalesStr.isEmpty()) {
          TcLog.d(TAG, "Ignoring " + file.getAbsolutePath());
          return null;
        }
        final List<Locale> supportedLocales = new ArrayList<>();
        for (String langTag : Splitter.on(',').split(supportedLocalesStr)) {
          supportedLocales.add(Locale.forLanguageTag(langTag));
        }
        return new ModelFile(
            modelType,
            file,
            version,
            supportedLocales,
            supportedLocalesStr,
            ModelFile.LANGUAGE_INDEPENDENT.equals(supportedLocalesStr));
      } catch (FileNotFoundException e) {
        TcLog.e(TAG, "Failed to find " + file.getAbsolutePath(), e);
        return null;
      } finally {
        maybeCloseAndLogError(modelFd);
      }
    }

    /** Closes the ParcelFileDescriptor, if non-null, and logs any errors that occur. */
    private static void maybeCloseAndLogError(@Nullable ParcelFileDescriptor fd) {
      if (fd == null) {
        return;
      }
      try {
        fd.close();
      } catch (IOException e) {
        TcLog.e(TAG, "Error closing file.", e);
      }
    }
  }

  /** Describes TextClassifier model files on disk. */
  public static final class ModelFile {
    public static final String LANGUAGE_INDEPENDENT = "*";

    @ModelType.ModelTypeDef private final String modelType;
    private final File file;
    private final int version;
    private final List<Locale> supportedLocales;
    private final String supportedLocalesStr;
    private final boolean languageIndependent;

    public ModelFile(
        @ModelType.ModelTypeDef String modelType,
        File file,
        int version,
        List<Locale> supportedLocales,
        String supportedLocalesStr,
        boolean languageIndependent) {
      this.modelType = Preconditions.checkNotNull(modelType);
      this.file = Preconditions.checkNotNull(file);
      this.version = version;
      this.supportedLocales = Preconditions.checkNotNull(supportedLocales);
      this.supportedLocalesStr = Preconditions.checkNotNull(supportedLocalesStr);
      this.languageIndependent = languageIndependent;
    }

    /** Returns the type of this model, defined in {@link ModelType}. */
    @ModelType.ModelTypeDef
    public String getModelType() {
      return modelType;
    }

    /** Returns the absolute path to the model file. */
    public String getPath() {
      return file.getAbsolutePath();
    }

    /** Returns a name to use for id generation, effectively the name of the model file. */
    public String getName() {
      return file.getName();
    }

    /** Returns the version tag in the model's metadata. */
    public int getVersion() {
      return version;
    }

    /** Returns whether the language supports any language in the given ranges. */
    public boolean isAnyLanguageSupported(List<Locale.LanguageRange> languageRanges) {
      Preconditions.checkNotNull(languageRanges);
      return languageIndependent || Locale.lookup(languageRanges, supportedLocales) != null;
    }

    /** Returns if this model file is preferred to the given one. */
    public boolean isPreferredTo(@Nullable ModelFile model) {
      // A model is preferred to no model.
      if (model == null) {
        return true;
      }

      // A language-specific model is preferred to a language independent
      // model.
      if (!languageIndependent && model.languageIndependent) {
        return true;
      }
      if (languageIndependent && !model.languageIndependent) {
        return false;
      }

      // A higher-version model is preferred.
      if (version > model.getVersion()) {
        return true;
      }
      return false;
    }

    @Override
    public int hashCode() {
      return Objects.hash(getPath());
    }

    @Override
    public boolean equals(Object other) {
      if (this == other) {
        return true;
      }
      if (other instanceof ModelFile) {
        final ModelFile otherModel = (ModelFile) other;
        return TextUtils.equals(getPath(), otherModel.getPath());
      }
      return false;
    }

    public ModelInfo toModelInfo() {
      return new ModelInfo(getVersion(), supportedLocalesStr);
    }

    @Override
    public String toString() {
      return String.format(
          Locale.US,
          "ModelFile { type=%s path=%s name=%s version=%d locales=%s }",
          modelType,
          getPath(),
          getName(),
          version,
          supportedLocalesStr);
    }

    public static ImmutableList<Optional<ModelInfo>> toModelInfos(
        Optional<ModelFile>... modelFiles) {
      return Arrays.stream(modelFiles)
          .map(modelFile -> modelFile.transform(ModelFile::toModelInfo))
          .collect(Collectors.collectingAndThen(Collectors.toList(), ImmutableList::copyOf));
    }

    /** Effectively an enum class to represent types of models. */
    public static final class ModelType {
      @Retention(RetentionPolicy.SOURCE)
      @StringDef({ANNOTATOR, LANG_ID, ACTIONS_SUGGESTIONS})
      public @interface ModelTypeDef {}

      public static final String ANNOTATOR = "annotator";
      public static final String LANG_ID = "lang_id";
      public static final String ACTIONS_SUGGESTIONS = "actions_suggestions";

      public static final ImmutableList<String> VALUES =
          ImmutableList.of(ANNOTATOR, LANG_ID, ACTIONS_SUGGESTIONS);

      public static ImmutableList<String> values() {
        return VALUES;
      }

      private ModelType() {}
    }
  }
}
