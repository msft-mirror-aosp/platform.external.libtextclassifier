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

import androidx.test.filters.SmallTest;

import com.google.android.collect.Sets;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import java.util.Arrays;
import java.util.Set;

@SmallTest
public class LanguageProficiencyEvaluatorTest {
    private static final float EPSILON = 0.01f;
    private LanguageProficiencyEvaluator mLanguageProficiencyEvaluator;

    @Mock private SystemLanguagesProvider mSystemLanguagesProvider;

    private static final String SYSTEM_LANGUAGE_EN = "en";
    private static final String SYSTEM_LANGUAGE_ZH = "zh";
    private static final String NORMAL_LANGUAGE_JP = "jp";
    private static final String NORMAL_LANGUAGE_FR = "fr";
    private static final String NORMAL_LANGUAGE_PL = "pl";
    private static final Set<String> EVALUATION_LANGUAGES =
            Sets.newArraySet(
                    SYSTEM_LANGUAGE_EN,
                    SYSTEM_LANGUAGE_ZH,
                    NORMAL_LANGUAGE_JP,
                    NORMAL_LANGUAGE_FR,
                    NORMAL_LANGUAGE_PL);

    @Mock private LanguageProficiencyAnalyzer mLanguageProficiencyAnalyzer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mSystemLanguagesProvider.getSystemLanguageTags())
                .thenReturn(Arrays.asList(SYSTEM_LANGUAGE_EN, SYSTEM_LANGUAGE_ZH));
        mLanguageProficiencyEvaluator = new LanguageProficiencyEvaluator(mSystemLanguagesProvider);
    }

    @Test
    public void evaluate_allCorrect() {
        when(mLanguageProficiencyAnalyzer.canUnderstand(Mockito.anyString()))
                .thenAnswer(
                        (Answer<Float>)
                                invocation -> {
                                    String languageTag = invocation.getArgument(0);
                                    if (languageTag.equals(SYSTEM_LANGUAGE_EN)
                                            || languageTag.equals(SYSTEM_LANGUAGE_ZH)) {
                                        return 1f;
                                    }
                                    return 0f;
                                });

        LanguageProficiencyEvaluator.EvaluationResult evaluationResult =
                mLanguageProficiencyEvaluator.evaluate(
                        mLanguageProficiencyAnalyzer, EVALUATION_LANGUAGES);

        assertThat(evaluationResult.truePositive).isEqualTo(2);
        assertThat(evaluationResult.trueNegative).isEqualTo(3);
        assertThat(evaluationResult.falsePositive).isEqualTo(0);
        assertThat(evaluationResult.falseNegative).isEqualTo(0);
        assertThat(evaluationResult.computePrecisionOfPositiveClass()).isWithin(EPSILON).of(1f);
        assertThat(evaluationResult.computePrecisionOfNegativeClass()).isWithin(EPSILON).of(1f);
        assertThat(evaluationResult.computeRecallOfPositiveClass()).isWithin(EPSILON).of(1f);
        assertThat(evaluationResult.computeRecallOfNegativeClass()).isWithin(EPSILON).of(1f);
        assertThat(evaluationResult.computeF1ScoreOfPositiveClass()).isWithin(EPSILON).of(1f);
        assertThat(evaluationResult.computeF1ScoreOfNegativeClass()).isWithin(EPSILON).of(1f);
    }

    @Test
    public void evaluate_allWrong() {
        when(mLanguageProficiencyAnalyzer.canUnderstand(Mockito.anyString()))
                .thenAnswer(
                        (Answer<Float>)
                                invocation -> {
                                    String languageTag = invocation.getArgument(0);
                                    if (languageTag.equals(SYSTEM_LANGUAGE_EN)
                                            || languageTag.equals(SYSTEM_LANGUAGE_ZH)) {
                                        return 0f;
                                    }
                                    return 1f;
                                });

        LanguageProficiencyEvaluator.EvaluationResult evaluationResult =
                mLanguageProficiencyEvaluator.evaluate(
                        mLanguageProficiencyAnalyzer, EVALUATION_LANGUAGES);

        assertThat(evaluationResult.truePositive).isEqualTo(0);
        assertThat(evaluationResult.trueNegative).isEqualTo(0);
        assertThat(evaluationResult.falsePositive).isEqualTo(3);
        assertThat(evaluationResult.falseNegative).isEqualTo(2);
        assertThat(evaluationResult.computePrecisionOfPositiveClass()).isWithin(EPSILON).of(0f);
        assertThat(evaluationResult.computePrecisionOfNegativeClass()).isWithin(EPSILON).of(0f);
        assertThat(evaluationResult.computeRecallOfPositiveClass()).isWithin(EPSILON).of(0f);
        assertThat(evaluationResult.computeRecallOfNegativeClass()).isWithin(EPSILON).of(0f);
        assertThat(evaluationResult.computeF1ScoreOfPositiveClass()).isWithin(EPSILON).of(0f);
        assertThat(evaluationResult.computeF1ScoreOfNegativeClass()).isWithin(EPSILON).of(0f);
    }

    @Test
    public void evaluate_mixed() {
        when(mLanguageProficiencyAnalyzer.canUnderstand(Mockito.anyString()))
                .thenAnswer(
                        (Answer<Float>)
                                invocation -> {
                                    String languageTag = invocation.getArgument(0);
                                    switch (languageTag) {
                                        case SYSTEM_LANGUAGE_EN:
                                            return 1f;
                                        case SYSTEM_LANGUAGE_ZH:
                                            return 0f;
                                        case NORMAL_LANGUAGE_FR:
                                            return 0f;
                                        case NORMAL_LANGUAGE_JP:
                                            return 0f;
                                        case NORMAL_LANGUAGE_PL:
                                            return 1f;
                                    }
                                    throw new IllegalArgumentException(
                                            "unexpected language: " + languageTag);
                                });

        LanguageProficiencyEvaluator.EvaluationResult evaluationResult =
                mLanguageProficiencyEvaluator.evaluate(
                        mLanguageProficiencyAnalyzer, EVALUATION_LANGUAGES);

        assertThat(evaluationResult.truePositive).isEqualTo(1);
        assertThat(evaluationResult.trueNegative).isEqualTo(2);
        assertThat(evaluationResult.falsePositive).isEqualTo(1);
        assertThat(evaluationResult.falseNegative).isEqualTo(1);
        assertThat(evaluationResult.computePrecisionOfPositiveClass()).isWithin(EPSILON).of(0.5f);
        assertThat(evaluationResult.computePrecisionOfNegativeClass()).isWithin(EPSILON).of(0.66f);
        assertThat(evaluationResult.computeRecallOfPositiveClass()).isWithin(EPSILON).of(0.5f);
        assertThat(evaluationResult.computeRecallOfNegativeClass()).isWithin(EPSILON).of(0.66f);
        assertThat(evaluationResult.computeF1ScoreOfPositiveClass()).isWithin(EPSILON).of(0.5f);
        assertThat(evaluationResult.computeF1ScoreOfNegativeClass()).isWithin(EPSILON).of(0.66f);
    }
}
