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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import androidx.room.Room;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.android.textclassifier.TextClassificationConstants;
import com.android.textclassifier.ulp.database.LanguageProfileDatabase;
import com.android.textclassifier.ulp.database.LanguageSignalInfo;
import java.util.Arrays;
import java.util.Collections;
import java.util.Locale;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@SmallTest
@RunWith(AndroidJUnit4.class)
/** Testing {@link KmeansLanguageProficiencyAnalyzer} in an in-memory database. */
public class KmeansLanguageProficiencyAnalyzerTest {

  private static final String PRIMARY_SYSTEM_LANGUAGE = Locale.CHINESE.toLanguageTag();
  private static final String SECONDARY_SYSTEM_LANGUAGE = Locale.ENGLISH.toLanguageTag();
  private static final String NORMAL_LANGUAGE = Locale.JAPANESE.toLanguageTag();

  private LanguageProfileDatabase mDatabase;
  private KmeansLanguageProficiencyAnalyzer mProficiencyAnalyzer;
  @Mock private SystemLanguagesProvider mSystemLanguagesProvider;

  @Before
  public void setup() {
    MockitoAnnotations.initMocks(this);

    Context context = ApplicationProvider.getApplicationContext();
    TextClassificationConstants textClassificationConstants =
        mock(TextClassificationConstants.class);
    mDatabase = Room.inMemoryDatabaseBuilder(context, LanguageProfileDatabase.class).build();
    mProficiencyAnalyzer =
        new KmeansLanguageProficiencyAnalyzer(
            textClassificationConstants, mDatabase, mSystemLanguagesProvider);
    when(mSystemLanguagesProvider.getSystemLanguageTags())
        .thenReturn(Arrays.asList(PRIMARY_SYSTEM_LANGUAGE, SECONDARY_SYSTEM_LANGUAGE));
    when(textClassificationConstants.getLanguageProficiencyBootstrappingCount()).thenReturn(100);
  }

  @After
  public void close() {
    mDatabase.close();
  }

  @Test
  public void canUnderstand_emptyDatabase() {
    assertThat(mProficiencyAnalyzer.canUnderstand(PRIMARY_SYSTEM_LANGUAGE)).isEqualTo(1f);
    assertThat(mProficiencyAnalyzer.canUnderstand(SECONDARY_SYSTEM_LANGUAGE)).isEqualTo(0.5f);
    assertThat(mProficiencyAnalyzer.canUnderstand(NORMAL_LANGUAGE)).isEqualTo(0f);
  }

  @Test
  public void canUnderstand_oneLanguage() {
    when(mSystemLanguagesProvider.getSystemLanguageTags())
        .thenReturn(Collections.singletonList(PRIMARY_SYSTEM_LANGUAGE));
    mDatabase
        .languageInfoDao()
        .insertLanguageInfo(
            new LanguageSignalInfo(
                PRIMARY_SYSTEM_LANGUAGE, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 1));

    assertThat(mProficiencyAnalyzer.canUnderstand(PRIMARY_SYSTEM_LANGUAGE)).isEqualTo(1f);
    assertThat(mProficiencyAnalyzer.canUnderstand(SECONDARY_SYSTEM_LANGUAGE)).isEqualTo(0f);
    assertThat(mProficiencyAnalyzer.canUnderstand(NORMAL_LANGUAGE)).isEqualTo(0f);
  }

  @Test
  public void canUnderstand_twoLanguages() {
    mDatabase
        .languageInfoDao()
        .insertLanguageInfo(
            new LanguageSignalInfo(
                PRIMARY_SYSTEM_LANGUAGE, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 100));
    mDatabase
        .languageInfoDao()
        .insertLanguageInfo(
            new LanguageSignalInfo(
                SECONDARY_SYSTEM_LANGUAGE, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 50));

    assertThat(mProficiencyAnalyzer.canUnderstand(PRIMARY_SYSTEM_LANGUAGE)).isEqualTo(1f);
    assertThat(mProficiencyAnalyzer.canUnderstand(SECONDARY_SYSTEM_LANGUAGE)).isEqualTo(0.5f);
    assertThat(mProficiencyAnalyzer.canUnderstand(NORMAL_LANGUAGE)).isEqualTo(0f);
  }

  @Test
  public void canUnderstand_threeLanguages() {
    mDatabase
        .languageInfoDao()
        .insertLanguageInfo(
            new LanguageSignalInfo(
                PRIMARY_SYSTEM_LANGUAGE, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 100));
    mDatabase
        .languageInfoDao()
        .insertLanguageInfo(
            new LanguageSignalInfo(
                SECONDARY_SYSTEM_LANGUAGE, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 75));
    mDatabase
        .languageInfoDao()
        .insertLanguageInfo(
            new LanguageSignalInfo(
                NORMAL_LANGUAGE, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 2));

    assertThat(mProficiencyAnalyzer.canUnderstand(PRIMARY_SYSTEM_LANGUAGE)).isEqualTo(1f);
    assertThat(mProficiencyAnalyzer.canUnderstand(SECONDARY_SYSTEM_LANGUAGE)).isEqualTo(1f);
    assertThat(mProficiencyAnalyzer.canUnderstand(NORMAL_LANGUAGE)).isEqualTo(0f);
  }
}
