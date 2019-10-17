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
import com.android.textclassifier.TextClassificationConstants;
import com.android.textclassifier.ulp.database.LanguageProfileDatabase;
import com.android.textclassifier.ulp.database.LanguageSignalInfo;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * A baseline class of {@link LanguageProficiencyAnalyzer}.
 *
 * <p>This analyzer takes message counts of different languages as signal. To bootstrap the model,
 * the analyzer adds extra count to the device languages. The counts are then normalized to the
 * range 0 to 1 and become the confidence scores. Language with higher confidence score that is the
 * language which the user is more familiar with.
 */
final class BasicLanguageProficiencyAnalyzer implements LanguageProficiencyAnalyzer {

  private static final long CAN_UNDERSTAND_RESULT_CACHE_EXPIRATION_TIME =
      TimeUnit.HOURS.toMillis(6);

  private final TextClassificationConstants settings;
  private final LanguageProfileDatabase database;
  private final SystemLanguagesProvider systemLanguagesProvider;

  private Map<String, Float> canUnderstandResultCache;
  private long canUnderstandResultCacheTime;

  BasicLanguageProficiencyAnalyzer(
      Context context,
      TextClassificationConstants settings,
      SystemLanguagesProvider systemLanguagesProvider) {
    this(settings, LanguageProfileDatabase.getInstance(context), systemLanguagesProvider);
  }

  @VisibleForTesting
  BasicLanguageProficiencyAnalyzer(
      TextClassificationConstants settings,
      LanguageProfileDatabase languageProfileDatabase,
      SystemLanguagesProvider systemLanguagesProvider) {
    this.settings = Preconditions.checkNotNull(settings);
    database = Preconditions.checkNotNull(languageProfileDatabase);
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

  @Override
  public void onTextClassifierEvent(TextClassifierEvent event) {}

  @Override
  public boolean shouldShowTranslation(String languageCode) {
    return canUnderstand(languageCode) >= settings.getTranslateActionThreshold();
  }

  private Map<String, Float> createCanUnderstandResultCache() {
    Map<String, Float> result = new ArrayMap<>();
    List<String> systemLanguageTags = systemLanguagesProvider.getSystemLanguageTags();
    List<LanguageSignalInfo> languageSignalInfos =
        database.languageInfoDao().getBySource(LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS);
    // Applies system languages to bootstrap the model according to Zipf's Law.
    // Zipf’s Law states that the ith most common language should be proportional to 1/i.
    for (int i = 0; i < systemLanguageTags.size(); i++) {
      String languageTag = systemLanguageTags.get(i);
      result.put(
          languageTag, settings.getLanguageProficiencyBootstrappingCount() / (float) (i + 1));
    }
    // Adds message counts of different languages into the corresponding entry in the map
    for (LanguageSignalInfo info : languageSignalInfos) {
      String languageTag = info.getLanguageTag();
      int count = info.getCount();
      result.put(languageTag, result.getOrDefault(languageTag, 0f) + count);
    }
    // Calculates confidence scores
    float max = Collections.max(result.values());
    result.forEach((languageTag, count) -> result.put(languageTag, count / max));
    return result;
  }
}
