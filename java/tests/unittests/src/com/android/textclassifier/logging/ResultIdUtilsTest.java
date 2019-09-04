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

package com.android.textclassifier.logging;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Test;

import java.util.Collections;
import java.util.Locale;

@SmallTest
public class ResultIdUtilsTest {
    private static final int MODEL_VERSION = 703;
    private static final int HASH = 12345;

    @Test
    public void createId_customHash() {
        String resultId =
                ResultIdUtils.createId(
                        MODEL_VERSION, Collections.singletonList(Locale.ENGLISH), HASH);

        assertThat(resultId).isEqualTo("androidtc|en_v703|12345");
    }

    @Test
    public void createId_selection() {
        String resultId =
                ResultIdUtils.createId(
                        ApplicationProvider.getApplicationContext(),
                        "text",
                        1,
                        2,
                        MODEL_VERSION,
                        Collections.singletonList(Locale.ENGLISH));

        assertThat(resultId).matches("androidtc\\|en_v703\\|-?\\d+");
    }
}
