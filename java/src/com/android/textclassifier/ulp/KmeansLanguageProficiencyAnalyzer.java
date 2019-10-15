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
import android.view.textclassifier.TextClassifierEvent;
import androidx.collection.ArrayMap;
import com.android.textclassifier.TextClassificationConstants;
import com.android.textclassifier.ulp.database.LanguageProfileDatabase;
import com.android.textclassifier.ulp.database.LanguageSignalInfo;
import com.android.textclassifier.ulp.kmeans.KMeans;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Uses K-Means algorithm to cluster data points.
 *
 * <p>This analyzer takes message counts of different languages as signal. To bootstrap the model,
 * the analyzer adds extra count to the device languages. It uses K-Means algorithm to put the data
 * points into two clusters. The languages in the cluster , which is farthest from the origin, will
 * be given a higher confidence score.
 */
// STOPSHIP: Review the entire ULP package before shipping it.
final class KmeansLanguageProficiencyAnalyzer implements LanguageProficiencyAnalyzer {

  private static final long CAN_UNDERSTAND_RESULT_CACHE_EXPIRATION_TIME =
      TimeUnit.HOURS.toMillis(6);

  private final TextClassificationConstants settings;
  private final LanguageProfileDatabase database;
  private final KMeans kmeans;
  private final SystemLanguagesProvider systemLanguagesProvider;

  private Map<String, Float> canUnderstandResultCache;
  private long canUnderstandResultCacheTime;

  KmeansLanguageProficiencyAnalyzer(
      Context context,
      TextClassificationConstants settings,
      SystemLanguagesProvider systemLanguagesProvider) {
    this(settings, LanguageProfileDatabase.getInstance(context), systemLanguagesProvider);
  }

  @VisibleForTesting
  KmeansLanguageProficiencyAnalyzer(
      TextClassificationConstants settings,
      LanguageProfileDatabase languageProfileDatabase,
      SystemLanguagesProvider systemLanguagesProvider) {
    this.settings = Preconditions.checkNotNull(settings);
    database = Preconditions.checkNotNull(languageProfileDatabase);
    kmeans = new KMeans();
    this.systemLanguagesProvider = Preconditions.checkNotNull(systemLanguagesProvider);
    canUnderstandResultCache = new ArrayMap<>();
  }

  @Override
  public synchronized float canUnderstand(String languageTag) {
    if (canUnderstandResultCache.isEmpty()
        || (System.currentTimeMillis() - canUnderstandResultCacheTime)
            >= CAN_UNDERSTAND_RESULT_CACHE_EXPIRATION_TIME) {
      canUnderstandResultCache = createCanUnderstandResultCache();
      canUnderstandResultCacheTime = System.currentTimeMillis();
    }
    return canUnderstandResultCache.getOrDefault(languageTag, 0f);
  }

  private Map<String, Float> createCanUnderstandResultCache() {
    Map<String, Float> result = new ArrayMap<>();
    ArrayMap<String, Integer> languageCounts = new ArrayMap<>();
    List<String> systemLanguageTags = systemLanguagesProvider.getSystemLanguageTags();
    List<LanguageSignalInfo> languageSignalInfos =
        database.languageInfoDao().getBySource(LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS);
    // Applies system languages to bootstrap the model according to Zipf's Law.
    // Zipf’s Law states that the ith most common language should be proportional to 1/i.
    for (int i = 0; i < systemLanguageTags.size(); i++) {
      String languageTag = systemLanguageTags.get(i);
      languageCounts.put(
          languageTag, settings.getLanguageProficiencyBootstrappingCount() / (i + 1));
    }
    // Adds message counts of different languages into the corresponding entry in the map
    for (LanguageSignalInfo info : languageSignalInfos) {
      String languageTag = info.getLanguageTag();
      int count = info.getCount();
      languageCounts.put(languageTag, languageCounts.getOrDefault(languageTag, 0) + count);
    }
    // Calculates confidence scores
    if (languageCounts.size() == 1) {
      result.put(languageCounts.keyAt(0), 1f);
      return result;
    }
    if (languageCounts.size() == 2) {
      return evaluateTwoLanguageCounts(languageCounts);
    }
    // Applies K-Means to cluster data points
    int size = languageCounts.size();
    float[][] inputData = new float[size][1];
    for (int i = 0; i < size; i++) {
      inputData[i][0] = languageCounts.valueAt(i);
    }
    List<KMeans.Mean> means = kmeans.predict(/* k= */ 2, inputData);
    List<Integer> countsInMaxCluster = getCountsWithinFarthestCluster(means);
    for (int i = 0; i < languageCounts.size(); i++) {
      float score = countsInMaxCluster.contains(languageCounts.valueAt(i)) ? 1f : 0f;
      result.put(languageCounts.keyAt(i), score);
    }
    return result;
  }

  @Override
  public void onTextClassifierEvent(TextClassifierEvent event) {}

  @Override
  public boolean shouldShowTranslation(String languageCode) {
    return canUnderstand(languageCode) >= settings.getTranslateActionThreshold();
  }

  private static Map<String, Float> evaluateTwoLanguageCounts(
      ArrayMap<String, Integer> languageCounts) {
    Map<String, Float> result = new ArrayMap<>();
    int countOne = languageCounts.valueAt(0);
    String languageTagOne = languageCounts.keyAt(0);
    int countTwo = languageCounts.valueAt(1);
    String languageTagTwo = languageCounts.keyAt(1);
    if (countOne >= countTwo) {
      result.put(languageTagOne, 1f);
      result.put(languageTagTwo, countTwo / (float) countOne);
    } else {
      result.put(languageTagTwo, 1f);
      result.put(languageTagOne, countOne / (float) countTwo);
    }
    return result;
  }

  private static List<Integer> getCountsWithinFarthestCluster(List<KMeans.Mean> means) {
    List<Integer> result = new ArrayList<>();
    KMeans.Mean farthestMean = means.get(0);
    for (int i = 1; i < means.size(); i++) {
      KMeans.Mean curMean = means.get(i);
      if (curMean.getCentroid()[0] > farthestMean.getCentroid()[0]) {
        farthestMean = curMean;
      }
    }
    for (float[] item : farthestMean.getItems()) {
      result.add((int) item[0]);
    }
    return result;
  }
}
