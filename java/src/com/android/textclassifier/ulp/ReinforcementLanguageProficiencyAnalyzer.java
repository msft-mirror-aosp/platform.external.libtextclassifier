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
import android.content.SharedPreferences;
import android.view.textclassifier.TextClassifierEvent;
import com.android.textclassifier.TcLog;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Class implementing reinforcement learning approach to learn user language preferences.
 *
 * <p>At the beginning, this analyzer assumes that the user doesn't know the language so that the
 * translation action should be shown. Based on whether user decides to make use of translation or
 * not, the analyzer updates information about user preferences and it may finally stop showing the
 * translation action in the future.
 */
class ReinforcementLanguageProficiencyAnalyzer implements LanguageProficiencyAnalyzer {
  private static final String TAG = "ReinforcementAnalyzer";
  private static final String PREF_NAME = "ulp-reinforcement-analyzer";
  private static final float SHOW_TRANSLATE_ACTION_THRESHOLD = 0.9f;
  private static final int MIN_NUM_TRANSLATE_SHOWN_TO_BE_CONFIDENT = 30;

  private final SystemLanguagesProvider systemLanguagesProvider;
  private final SharedPreferences sharedPreferences;

  ReinforcementLanguageProficiencyAnalyzer(
      Context context, SystemLanguagesProvider systemLanguagesProvider) {
    Preconditions.checkNotNull(context);
    this.systemLanguagesProvider = Preconditions.checkNotNull(systemLanguagesProvider);
    sharedPreferences = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
  }

  @VisibleForTesting
  ReinforcementLanguageProficiencyAnalyzer(
      SystemLanguagesProvider systemLanguagesProvider, SharedPreferences sharedPreferences) {
    this.systemLanguagesProvider = Preconditions.checkNotNull(systemLanguagesProvider);
    this.sharedPreferences = Preconditions.checkNotNull(sharedPreferences);
  }

  @Override
  public float canUnderstand(String languageTag) {
    TranslationStatistics translationStatistics =
        TranslationStatistics.loadFromSharedPreference(sharedPreferences, languageTag);
    if (translationStatistics.getShownCount() < MIN_NUM_TRANSLATE_SHOWN_TO_BE_CONFIDENT) {
      return systemLanguagesProvider.getSystemLanguageTags().contains(languageTag) ? 1f : 0f;
    }
    return translationStatistics.getScore();
  }

  @Override
  public boolean shouldShowTranslation(String languageTag) {
    TranslationStatistics translationStatistics =
        TranslationStatistics.loadFromSharedPreference(sharedPreferences, languageTag);
    if (translationStatistics.getShownCount() < MIN_NUM_TRANSLATE_SHOWN_TO_BE_CONFIDENT) {
      // Show translate action until we have enough feedback.
      return true;
    }
    return translationStatistics.getScore() <= SHOW_TRANSLATE_ACTION_THRESHOLD;
  }

  @Override
  public void onTextClassifierEvent(TextClassifierEvent event) {
    if (event.getEventCategory() == TextClassifierEvent.CATEGORY_LANGUAGE_DETECTION) {
      if (event.getEventType() == TextClassifierEvent.TYPE_SMART_ACTION
          || event.getEventType() == TextClassifierEvent.TYPE_ACTIONS_SHOWN) {
        onTranslateEvent(event);
      }
    }
  }

  private void onTranslateEvent(TextClassifierEvent event) {
    if (event.getEntityTypes().length == 0) {
      return;
    }
    String languageTag = event.getEntityTypes()[0];
    // We only count the case that we show translate action in the prime position.
    if (event.getActionIndices().length == 0 || event.getActionIndices()[0] != 0) {
      return;
    }
    TranslationStatistics translationStatistics =
        TranslationStatistics.loadFromSharedPreference(sharedPreferences, languageTag);
    if (event.getEventType() == TextClassifierEvent.TYPE_ACTIONS_SHOWN) {
      translationStatistics.increaseShownCountByOne();
    } else if (event.getEventType() == TextClassifierEvent.TYPE_SMART_ACTION) {
      translationStatistics.increaseClickedCountByOne();
    }
    translationStatistics.save(sharedPreferences, languageTag);
  }

  private static final class TranslationStatistics {
    private static final String SEEN_COUNT = "seen_count";
    private static final String CLICK_COUNT = "click_count";

    private int shownCount;
    private int clickCount;

    private TranslationStatistics() {
      this(/* seenCount= */ 0, /* clickCount= */ 0);
    }

    private TranslationStatistics(int seenCount, int clickCount) {
      shownCount = seenCount;
      this.clickCount = clickCount;
    }

    static TranslationStatistics loadFromSharedPreference(
        SharedPreferences sharedPreferences, String languageTag) {
      String serializedString = sharedPreferences.getString(languageTag, null);
      return TranslationStatistics.fromSerializedString(serializedString);
    }

    void save(SharedPreferences sharedPreferences, String languageTag) {
      // TODO: Consider to store it in a database.
      sharedPreferences.edit().putString(languageTag, serializeToString()).apply();
    }

    private String serializeToString() {
      try {
        JSONObject jsonObject = new JSONObject();
        jsonObject.put(SEEN_COUNT, shownCount);
        jsonObject.put(CLICK_COUNT, clickCount);
        return jsonObject.toString();
      } catch (JSONException ex) {
        TcLog.e(TAG, "serializeToString: ", ex);
      }
      return "";
    }

    void increaseShownCountByOne() {
      shownCount += 1;
    }

    void increaseClickedCountByOne() {
      clickCount += 1;
    }

    float getScore() {
      if (shownCount == 0) {
        return 0f;
      }
      return clickCount / (float) shownCount;
    }

    int getShownCount() {
      return shownCount;
    }

    static TranslationStatistics fromSerializedString(String str) {
      if (str == null) {
        return new TranslationStatistics();
      }
      try {
        JSONObject jsonObject = new JSONObject(str);
        int seenCount = jsonObject.getInt(SEEN_COUNT);
        int clickCount = jsonObject.getInt(CLICK_COUNT);
        return new TranslationStatistics(seenCount, clickCount);
      } catch (JSONException ex) {
        TcLog.e(TAG, "Failed to parse " + str, ex);
      }
      return new TranslationStatistics();
    }

    @Override
    public String toString() {
      return "TranslationStatistics{"
          + "mShownCount="
          + shownCount
          + ", mClickCount="
          + clickCount
          + '}';
    }
  }
}
