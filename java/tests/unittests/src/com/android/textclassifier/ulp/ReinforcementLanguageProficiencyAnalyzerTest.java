/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.SharedPreferences;
import android.view.textclassifier.TextClassifierEvent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.Arrays;
import java.util.Locale;

/** Testing {@link ReinforcementLanguageProficiencyAnalyzer} using Mockito. */
@SmallTest
public class ReinforcementLanguageProficiencyAnalyzerTest {

    private static final String PRIMARY_SYSTEM_LANGUAGE = Locale.CHINESE.toLanguageTag();
    private static final String SECONDARY_SYSTEM_LANGUAGE = Locale.ENGLISH.toLanguageTag();
    private static final String NON_SYSTEM_LANGUAGE = Locale.JAPANESE.toLanguageTag();
    private ReinforcementLanguageProficiencyAnalyzer mProficiencyAnalyzer;
    @Mock private SystemLanguagesProvider mSystemLanguagesProvider;
    private SharedPreferences mSharedPreferences;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        Context context = ApplicationProvider.getApplicationContext();
        mSharedPreferences = context.getSharedPreferences("test-preferences", Context.MODE_PRIVATE);
        when(mSystemLanguagesProvider.getSystemLanguageTags())
                .thenReturn(Arrays.asList(PRIMARY_SYSTEM_LANGUAGE, SECONDARY_SYSTEM_LANGUAGE));
        mProficiencyAnalyzer =
                new ReinforcementLanguageProficiencyAnalyzer(
                        mSystemLanguagesProvider, mSharedPreferences);
    }

    @After
    public void teardown() {
        mSharedPreferences.edit().clear().apply();
    }

    @Test
    public void canUnderstand_defaultValue() {
        assertThat(mProficiencyAnalyzer.canUnderstand(PRIMARY_SYSTEM_LANGUAGE)).isEqualTo(1.0f);
        assertThat(mProficiencyAnalyzer.canUnderstand(SECONDARY_SYSTEM_LANGUAGE)).isEqualTo(1.0f);
        assertThat(mProficiencyAnalyzer.canUnderstand(NON_SYSTEM_LANGUAGE)).isEqualTo(0f);
    }

    @Test
    public void canUnderstand_enoughFeedback() {
        sendEvent(TextClassifierEvent.TYPE_ACTIONS_SHOWN, PRIMARY_SYSTEM_LANGUAGE, /* times= */ 50);
        sendEvent(TextClassifierEvent.TYPE_SMART_ACTION, PRIMARY_SYSTEM_LANGUAGE, /* times= */ 40);

        assertThat(mProficiencyAnalyzer.canUnderstand(PRIMARY_SYSTEM_LANGUAGE)).isEqualTo(0.8f);
    }

    @Test
    public void shouldShowTranslation_defaultValue() {
        assertThat(mProficiencyAnalyzer.shouldShowTranslation(PRIMARY_SYSTEM_LANGUAGE))
                .isEqualTo(true);
        assertThat(mProficiencyAnalyzer.shouldShowTranslation(SECONDARY_SYSTEM_LANGUAGE))
                .isEqualTo(true);
        assertThat(mProficiencyAnalyzer.shouldShowTranslation(NON_SYSTEM_LANGUAGE)).isEqualTo(true);
    }

    @Test
    public void shouldShowTranslation_enoughFeedback_true() {
        sendEvent(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN, PRIMARY_SYSTEM_LANGUAGE, /* times= */ 1000);
        sendEvent(TextClassifierEvent.TYPE_SMART_ACTION, PRIMARY_SYSTEM_LANGUAGE, /* times= */ 200);

        assertThat(mProficiencyAnalyzer.shouldShowTranslation(PRIMARY_SYSTEM_LANGUAGE))
                .isEqualTo(true);
    }

    @Test
    public void shouldShowTranslation_enoughFeedback_false() {
        sendEvent(
                TextClassifierEvent.TYPE_ACTIONS_SHOWN, PRIMARY_SYSTEM_LANGUAGE, /* times= */ 1000);
        sendEvent(
                TextClassifierEvent.TYPE_SMART_ACTION, PRIMARY_SYSTEM_LANGUAGE, /* times= */ 1000);

        assertThat(mProficiencyAnalyzer.shouldShowTranslation(PRIMARY_SYSTEM_LANGUAGE))
                .isEqualTo(false);
    }

    private void sendEvent(int type, String languageTag, int times) {
        TextClassifierEvent.LanguageDetectionEvent event =
                new TextClassifierEvent.LanguageDetectionEvent.Builder(type)
                        .setEntityTypes(languageTag)
                        .setActionIndices(0)
                        .build();
        for (int i = 0; i < times; i++) {
            mProficiencyAnalyzer.onTextClassifierEvent(event);
        }
    }
}
