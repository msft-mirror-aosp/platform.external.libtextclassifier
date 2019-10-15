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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import androidx.room.Room;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.android.textclassifier.Entity;
import com.android.textclassifier.TextClassificationConstants;
import com.android.textclassifier.subjects.EntitySubject;
import com.android.textclassifier.ulp.database.LanguageProfileDatabase;
import com.android.textclassifier.ulp.database.LanguageSignalInfo;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@SmallTest
@RunWith(AndroidJUnit4.class)
/** Testing {@link LanguageProfileAnalyzer} in an inMemoryDatabase. */
public class LanguageProfileAnalyzerTest {

  private static final String SYSTEM_LANGUAGE_CODE = "en";
  private static final String LOCATION_LANGUAGE_CODE = "jp";
  private static final String NORMAL_LANGUAGE_CODE = "pl";

  private LanguageProfileDatabase mDatabase;
  private LanguageProfileAnalyzer mLanguageProfileAnalyzer;
  @Mock private LocationSignalProvider mLocationSignalProvider;
  @Mock private SystemLanguagesProvider mSystemLanguagesProvider;
  @Mock private LanguageProficiencyAnalyzer mLanguageProficiencyAnalyzer;

  @Before
  public void setup() {
    MockitoAnnotations.initMocks(this);

    Context mContext = ApplicationProvider.getApplicationContext();
    mDatabase = Room.inMemoryDatabaseBuilder(mContext, LanguageProfileDatabase.class).build();
    when(mLocationSignalProvider.detectLanguageTag()).thenReturn(LOCATION_LANGUAGE_CODE);
    when(mSystemLanguagesProvider.getSystemLanguageTags())
        .thenReturn(Collections.singletonList(SYSTEM_LANGUAGE_CODE));
    when(mLanguageProficiencyAnalyzer.canUnderstand(anyString())).thenReturn(1.0f);
    TextClassificationConstants customTextClassificationConstants =
        mock(TextClassificationConstants.class);
    when(customTextClassificationConstants.getFrequentLanguagesBootstrappingCount())
        .thenReturn(100);
    mLanguageProfileAnalyzer =
        new LanguageProfileAnalyzer(
            mContext,
            customTextClassificationConstants,
            mDatabase,
            mLanguageProficiencyAnalyzer,
            mLocationSignalProvider,
            mSystemLanguagesProvider);
  }

  @After
  public void close() {
    mDatabase.close();
  }

  @Test
  public void getFrequentLanguages_emptyDatabase() {
    List<Entity> frequentLanguages =
        mLanguageProfileAnalyzer.getFrequentLanguages(LanguageSignalInfo.CLASSIFY_TEXT);

    assertThat(frequentLanguages).hasSize(2);
    EntitySubject.assertThat(frequentLanguages.get(0))
        .isMatchWithinTolerance(new Entity(SYSTEM_LANGUAGE_CODE, 1.0f));
    EntitySubject.assertThat(frequentLanguages.get(1))
        .isMatchWithinTolerance(new Entity(LOCATION_LANGUAGE_CODE, 1.0f));
  }

  @Test
  public void getFrequentLanguages_mixedSignal() {
    insertSignal(NORMAL_LANGUAGE_CODE, LanguageSignalInfo.CLASSIFY_TEXT, 50);
    insertSignal(SYSTEM_LANGUAGE_CODE, LanguageSignalInfo.CLASSIFY_TEXT, 100);
    // Unrelated signals.
    insertSignal(NORMAL_LANGUAGE_CODE, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 100);
    insertSignal(SYSTEM_LANGUAGE_CODE, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 100);
    insertSignal(LOCATION_LANGUAGE_CODE, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 100);

    List<Entity> frequentLanguages =
        mLanguageProfileAnalyzer.getFrequentLanguages(LanguageSignalInfo.CLASSIFY_TEXT);

    assertThat(frequentLanguages).hasSize(3);
    EntitySubject.assertThat(frequentLanguages.get(0))
        .isMatchWithinTolerance(new Entity(SYSTEM_LANGUAGE_CODE, 1.0f));
    EntitySubject.assertThat(frequentLanguages.get(1))
        .isMatchWithinTolerance(new Entity(LOCATION_LANGUAGE_CODE, 0.5f));
    EntitySubject.assertThat(frequentLanguages.get(2))
        .isMatchWithinTolerance(new Entity(NORMAL_LANGUAGE_CODE, 0.25f));
  }

  @Test
  public void getFrequentLanguages_bothSystemLanguageAndLocationLanguage() {
    when(mLocationSignalProvider.detectLanguageTag()).thenReturn("en");
    when(mSystemLanguagesProvider.getSystemLanguageTags()).thenReturn(Arrays.asList("en", "jp"));

    List<Entity> frequentLanguages =
        mLanguageProfileAnalyzer.getFrequentLanguages(LanguageSignalInfo.CLASSIFY_TEXT);

    assertThat(frequentLanguages).hasSize(2);
    EntitySubject.assertThat(frequentLanguages.get(0))
        .isMatchWithinTolerance(new Entity("en", 1.0f));
    EntitySubject.assertThat(frequentLanguages.get(1))
        .isMatchWithinTolerance(new Entity("jp", 0.5f));
  }

  private void insertSignal(String languageTag, @LanguageSignalInfo.Source int source, int count) {
    mDatabase
        .languageInfoDao()
        .insertLanguageInfo(new LanguageSignalInfo(languageTag, source, count));
  }
}
