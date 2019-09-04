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

import androidx.collection.ArrayMap;
import androidx.collection.ArraySet;
import androidx.core.util.Preconditions;

import java.util.Set;

final class LanguageProficiencyEvaluator {
    private final SystemLanguagesProvider mSystemLanguagesProvider;

    LanguageProficiencyEvaluator(SystemLanguagesProvider systemLanguagesProvider) {
        mSystemLanguagesProvider = Preconditions.checkNotNull(systemLanguagesProvider);
    }

    EvaluationResult evaluate(
            LanguageProficiencyAnalyzer analyzer, Set<String> languagesToEvaluate) {
        Set<String> systemLanguageTags =
                new ArraySet<>(mSystemLanguagesProvider.getSystemLanguageTags());
        ArrayMap<String, Boolean> actual = new ArrayMap<>();
        // We assume user can only speak the languages that are set as system languages.
        for (String languageToEvaluate : languagesToEvaluate) {
            actual.put(languageToEvaluate, systemLanguageTags.contains(languageToEvaluate));
        }
        return evaluateWithActual(analyzer, actual);
    }

    private EvaluationResult evaluateWithActual(
            LanguageProficiencyAnalyzer analyzer, ArrayMap<String, Boolean> actual) {
        ArrayMap<String, Boolean> predict = new ArrayMap<>();
        for (int i = 0; i < actual.size(); i++) {
            String languageTag = actual.keyAt(i);
            predict.put(languageTag, analyzer.canUnderstand(languageTag) >= 0.5f);
        }
        return EvaluationResult.create(actual, predict);
    }

    static final class EvaluationResult {
        final int truePositive;
        final int trueNegative;
        final int falsePositive;
        final int falseNegative;

        private EvaluationResult(
                int truePositive, int trueNegative, int falsePositive, int falseNegative) {
            this.truePositive = truePositive;
            this.trueNegative = trueNegative;
            this.falsePositive = falsePositive;
            this.falseNegative = falseNegative;
        }

        float computePrecisionOfPositiveClass() {
            float divisor = truePositive + falsePositive;
            return divisor != 0 ? truePositive / divisor : 1f;
        }

        float computePrecisionOfNegativeClass() {
            float divisor = trueNegative + falseNegative;
            return divisor != 0 ? trueNegative / divisor : 1f;
        }

        float computeRecallOfPositiveClass() {
            float divisor = truePositive + falseNegative;
            return divisor != 0 ? truePositive / divisor : 1f;
        }

        float computeRecallOfNegativeClass() {
            float divisor = trueNegative + falsePositive;
            return divisor != 0 ? trueNegative / divisor : 1f;
        }

        float computeF1ScoreOfPositiveClass() {
            return 2 * truePositive / (float) (2 * truePositive + falsePositive + falseNegative);
        }

        float computeF1ScoreOfNegativeClass() {
            return 2 * trueNegative / (float) (2 * trueNegative + falsePositive + falseNegative);
        }

        static EvaluationResult create(
                ArrayMap<String, Boolean> actual, ArrayMap<String, Boolean> predict) {
            int truePositive = 0;
            int trueNegative = 0;
            int falsePositive = 0;
            int falseNegative = 0;
            for (int i = 0; i < actual.size(); i++) {
                String languageTag = actual.keyAt(i);
                boolean actualLabel = actual.valueAt(i);
                boolean predictLabel = predict.get(languageTag);
                if (predictLabel) {
                    if (actualLabel == predictLabel) {
                        truePositive += 1;
                    } else {
                        falsePositive += 1;
                    }
                } else {
                    if (actualLabel == predictLabel) {
                        trueNegative += 1;
                    } else {
                        falseNegative += 1;
                    }
                }
            }
            return new EvaluationResult(truePositive, trueNegative, falsePositive, falseNegative);
        }
    }
}
