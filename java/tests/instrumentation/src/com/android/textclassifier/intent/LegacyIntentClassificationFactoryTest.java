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

package com.android.textclassifier.intent;

import static com.google.common.truth.Truth.assertThat;

import android.content.Intent;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.google.android.textclassifier.AnnotatorModel;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class LegacyIntentClassificationFactoryTest {

  private static final String TEXT = "text";
  private static final String TYPE_DICTIONARY = "dictionary";

  private LegacyClassificationIntentFactory legacyIntentClassificationFactory;

  @Before
  public void setup() {
    legacyIntentClassificationFactory = new LegacyClassificationIntentFactory();
  }

  @Test
  public void create_typeDictionary() {
    AnnotatorModel.ClassificationResult classificationResult =
        new AnnotatorModel.ClassificationResult(
            TYPE_DICTIONARY,
            1.0f,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            0L,
            0L,
            0d);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText */ false,
            null,
            classificationResult);

    assertThat(intents).hasSize(1);
    LabeledIntent labeledIntent = intents.get(0);
    Intent intent = labeledIntent.intent;
    assertThat(intent.getAction()).isEqualTo(Intent.ACTION_DEFINE);
    assertThat(intent.getStringExtra(Intent.EXTRA_TEXT)).isEqualTo(TEXT);
  }

  @Test
  public void create_translateAndDictionary() {
    AnnotatorModel.ClassificationResult classificationResult =
        new AnnotatorModel.ClassificationResult(
            TYPE_DICTIONARY,
            1.0f,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            null,
            0L,
            0L,
            0d);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText */ true,
            null,
            classificationResult);

    assertThat(intents).hasSize(2);
    assertThat(intents.get(0).intent.getAction()).isEqualTo(Intent.ACTION_DEFINE);
    assertThat(intents.get(1).intent.getAction()).isEqualTo(Intent.ACTION_TRANSLATE);
  }
}