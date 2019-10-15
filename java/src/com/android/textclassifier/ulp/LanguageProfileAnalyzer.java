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

package com.android.textclassifier.ulp;

import android.content.Context;
import android.util.ArrayMap;
import android.view.textclassifier.TextClassifierEvent;
import androidx.annotation.FloatRange;
import com.android.textclassifier.Entity;
import com.android.textclassifier.TcLog;
import com.android.textclassifier.TextClassificationConstants;
import com.android.textclassifier.ulp.database.LanguageProfileDatabase;
import com.android.textclassifier.ulp.database.LanguageSignalInfo;
import com.android.textclassifier.utils.IndentingPrintWriter;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableList;
import com.google.common.util.concurrent.MoreExecutors;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.stream.Collectors;

/**
 * Default class implementing functions which analyze user language profile.
 *
 * <p>This class uses heuristics to decide based on collected data whether user understands passed
 * language or often sees it on their phone. Unless otherwise stated, methods of this class are
 * blocking operations and should be called on the worker thread.
 */
public class LanguageProfileAnalyzer {
  private final Context context;
  private final TextClassificationConstants textClassificationConstants;
  private final LanguageProfileDatabase languageProfileDatabase;
  private final LanguageProficiencyAnalyzer proficiencyAnalyzer;
  private final LocationSignalProvider locationSignalProvider;
  private final SystemLanguagesProvider systemLanguagesProvider;

  @VisibleForTesting
  LanguageProfileAnalyzer(
      Context context,
      TextClassificationConstants textClassificationConstants,
      LanguageProfileDatabase database,
      LanguageProficiencyAnalyzer languageProficiencyAnalyzer,
      LocationSignalProvider locationSignalProvider,
      SystemLanguagesProvider systemLanguagesProvider) {
    this.context = context;
    this.textClassificationConstants = textClassificationConstants;
    languageProfileDatabase = Preconditions.checkNotNull(database);
    proficiencyAnalyzer = Preconditions.checkNotNull(languageProficiencyAnalyzer);
    this.locationSignalProvider = Preconditions.checkNotNull(locationSignalProvider);
    this.systemLanguagesProvider = Preconditions.checkNotNull(systemLanguagesProvider);
  }

  /** Creates an instance of {@link LanguageProfileAnalyzer}. */
  public static LanguageProfileAnalyzer create(
      Context context, TextClassificationConstants textClassificationConstants) {
    SystemLanguagesProvider systemLanguagesProvider = new SystemLanguagesProvider();
    LocationSignalProvider locationSignalProvider = new LocationSignalProvider(context);
    return new LanguageProfileAnalyzer(
        context,
        textClassificationConstants,
        LanguageProfileDatabase.getInstance(context),
        new ReinforcementLanguageProficiencyAnalyzer(context, systemLanguagesProvider),
        locationSignalProvider,
        systemLanguagesProvider);
  }

  /**
   * Returns the confidence score for which the user understands the given language. The result is
   * recalculated every constant time.
   *
   * <p>The score ranges from 0 to 1. 1 indicates the language is very familiar to the user and vice
   * versa.
   */
  @FloatRange(from = 0.0, to = 1.0)
  public float canUnderstand(String languageTag) {
    return proficiencyAnalyzer.canUnderstand(languageTag);
  }

  /** Decides whether we should show translation for that language or no. */
  public boolean shouldShowTranslation(String languageTag) {
    return proficiencyAnalyzer.shouldShowTranslation(languageTag);
  }

  /** Performs actions defined for specific TextClassification events. */
  public void onTextClassifierEven(TextClassifierEvent event) {
    proficiencyAnalyzer.onTextClassifierEvent(event);
  }

  /**
   * Returns a list of languages that appear in the specified source, the list is sorted by the
   * frequency descendingly. The confidence score represents how frequent of the language is,
   * compared to the most frequent language.
   */
  public List<Entity> getFrequentLanguages(@LanguageSignalInfo.Source int source) {
    List<LanguageSignalInfo> languageSignalInfos =
        languageProfileDatabase.languageInfoDao().getBySource(source);
    int bootstrappingCount = textClassificationConstants.getFrequentLanguagesBootstrappingCount();
    ArrayMap<String, Integer> languageCountMap = new ArrayMap<>();
    systemLanguagesProvider
        .getSystemLanguageTags()
        .forEach(lang -> languageCountMap.put(lang, bootstrappingCount));
    String languageTagFromLocation = locationSignalProvider.detectLanguageTag();
    if (languageTagFromLocation != null) {
      languageCountMap.put(
          languageTagFromLocation,
          languageCountMap.getOrDefault(languageTagFromLocation, 0) + bootstrappingCount);
    }
    for (LanguageSignalInfo languageSignalInfo : languageSignalInfos) {
      String lang = languageSignalInfo.getLanguageTag();
      languageCountMap.put(
          lang, languageSignalInfo.getCount() + languageCountMap.getOrDefault(lang, 0));
    }
    int max = Collections.max(languageCountMap.values());
    if (max == 0) {
      return ImmutableList.of();
    }
    List<Entity> frequentLanguages = new ArrayList<>();
    for (int i = 0; i < languageCountMap.size(); i++) {
      String lang = languageCountMap.keyAt(i);
      float score = languageCountMap.valueAt(i) / (float) max;
      frequentLanguages.add(new Entity(lang, score));
    }
    Collections.sort(frequentLanguages);
    return ImmutableList.copyOf(frequentLanguages);
  }

  /** Dumps the data on the screen when called. */
  public void dump(IndentingPrintWriter printWriter) {
    printWriter.println("LanguageProfileAnalyzer:");
    printWriter.increaseIndent();
    printWriter.printPair(
        "System languages", String.join(",", systemLanguagesProvider.getSystemLanguageTags()));
    printWriter.printPair(
        "Language code deduced from location", locationSignalProvider.detectLanguageTag());

    ExecutorService executorService =
        MoreExecutors.listeningDecorator(Executors.newSingleThreadExecutor());
    try {
      executorService
          .submit(
              () -> {
                printWriter.println("Languages that user has seen in selections:");
                dumpFrequentLanguages(printWriter, LanguageSignalInfo.CLASSIFY_TEXT);

                printWriter.println("Languages that user has seen in message notifications:");
                dumpFrequentLanguages(printWriter, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS);

                dumpEvaluationReport(printWriter);
              })
          .get();
    } catch (ExecutionException | InterruptedException e) {
      TcLog.e(TcLog.TAG, "Dumping interrupted: ", e);
    }
    printWriter.decreaseIndent();
  }

  private void dumpEvaluationReport(IndentingPrintWriter printWriter) {
    List<String> systemLanguageTags = systemLanguagesProvider.getSystemLanguageTags();
    if (systemLanguageTags.size() <= 1) {
      printWriter.println("Skipped evaluation as there are less than two system languages.");
      return;
    }
    Set<String> languagesToEvaluate =
        languageProfileDatabase.languageInfoDao().getAll().stream()
            .map(LanguageSignalInfo::getLanguageTag)
            .collect(Collectors.toSet());
    languagesToEvaluate.addAll(systemLanguageTags);
    LanguageProficiencyEvaluator evaluator =
        new LanguageProficiencyEvaluator(systemLanguagesProvider);
    LanguageProficiencyAnalyzer[] analyzers =
        new LanguageProficiencyAnalyzer[] {
          new BasicLanguageProficiencyAnalyzer(
              context, textClassificationConstants, systemLanguagesProvider),
          new KmeansLanguageProficiencyAnalyzer(
              context, textClassificationConstants, systemLanguagesProvider),
          proficiencyAnalyzer
        };
    for (LanguageProficiencyAnalyzer analyzer : analyzers) {
      LanguageProficiencyEvaluator.EvaluationResult result =
          evaluator.evaluate(analyzer, languagesToEvaluate);
      printWriter.println("Evaluation result of " + analyzer.getClass().getSimpleName());
      printWriter.increaseIndent();
      printWriter.printPair(
          "Precision of positive class", result.computePrecisionOfPositiveClass());
      printWriter.printPair(
          "Precision of negative class", result.computePrecisionOfNegativeClass());
      printWriter.printPair("Recall of positive class", result.computeRecallOfPositiveClass());
      printWriter.printPair("Recall of negative class", result.computeRecallOfNegativeClass());
      printWriter.printPair("F1 score of positive class", result.computeF1ScoreOfPositiveClass());
      printWriter.printPair("F1 score of negative class", result.computeF1ScoreOfNegativeClass());
      printWriter.decreaseIndent();
    }
  }

  private void dumpFrequentLanguages(
      IndentingPrintWriter printWriter, @LanguageSignalInfo.Source int source) {
    printWriter.increaseIndent();
    for (Entity frequentLanguage : getFrequentLanguages(source)) {
      printWriter.println(frequentLanguage.toString());
    }
    printWriter.decreaseIndent();
  }
}
