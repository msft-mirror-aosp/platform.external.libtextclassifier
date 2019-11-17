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

package com.android.textclassifier.common.logging;

import static com.google.common.truth.Truth.assertThat;

import android.view.textclassifier.TextClassifier;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class TextClassificationContextTest {

  @Test
  public void minimumObject() {
    TextClassificationContext textClassificationContext =
        new TextClassificationContext.Builder("pkg", TextClassifier.WIDGET_TYPE_EDITTEXT).build();

    assertThat(textClassificationContext.getPackageName()).isEqualTo("pkg");
    assertThat(textClassificationContext.getWidgetType())
        .isEqualTo(TextClassifier.WIDGET_TYPE_EDITTEXT);
  }

  @Test
  public void fullObject() {
    TextClassificationContext textClassificationContext =
        new TextClassificationContext.Builder("pkg", TextClassifier.WIDGET_TYPE_EDITTEXT)
            .setWidgetVersion("v1")
            .build();

    assertThat(textClassificationContext.getPackageName()).isEqualTo("pkg");
    assertThat(textClassificationContext.getWidgetType())
        .isEqualTo(TextClassifier.WIDGET_TYPE_EDITTEXT);
    assertThat(textClassificationContext.getWidgetVersion()).isEqualTo("v1");
  }
}
