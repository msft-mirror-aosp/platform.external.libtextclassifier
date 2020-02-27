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
import android.view.textclassifier.TextClassifier;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.google.android.textclassifier.AnnotatorModel;
import com.google.android.textclassifier.AnnotatorModel.ClassificationResult;
import com.google.android.textclassifier.AnnotatorModel.DatetimeResult;
import java.time.Instant;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class LegacyIntentClassificationFactoryTest {

  private static final String TEXT = "text";
  // This one is hidden in Android source: http://shortn/_yTWS4pzb0P
  private static final String TYPE_DICTIONARY = "dictionary";

  private LegacyClassificationIntentFactory legacyIntentClassificationFactory;

  @Before
  public void setup() {
    legacyIntentClassificationFactory = new LegacyClassificationIntentFactory();
  }

  @Test
  public void create_email() {
    AnnotatorModel.ClassificationResult classificationResult =
        createClassificationResult(TextClassifier.TYPE_EMAIL);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText= */ false,
            /* referenceTime= */ null,
            classificationResult);

    assertThat(intents).hasSize(2);
    assertThat(intents.get(0).intent.getAction()).isEqualTo(Intent.ACTION_SENDTO);
    assertThat(intents.get(1).intent.getAction()).isEqualTo(Intent.ACTION_INSERT_OR_EDIT);
  }

  @Test
  public void create_phone() {
    AnnotatorModel.ClassificationResult classificationResult =
        createClassificationResult(TextClassifier.TYPE_PHONE);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText= */ false,
            /* referenceTime= */ null,
            classificationResult);

    assertThat(intents).hasSize(3);
    assertThat(intents.get(0).intent.getAction()).isEqualTo(Intent.ACTION_DIAL);
    assertThat(intents.get(1).intent.getAction()).isEqualTo(Intent.ACTION_INSERT_OR_EDIT);
    assertThat(intents.get(2).intent.getAction()).isEqualTo(Intent.ACTION_SENDTO);
  }

  @Test
  public void create_address() {
    AnnotatorModel.ClassificationResult classificationResult =
        createClassificationResult(TextClassifier.TYPE_ADDRESS);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText= */ false,
            /* referenceTime= */ null,
            classificationResult);

    assertThat(intents).hasSize(1);
    assertThat(intents.get(0).intent.getAction()).isEqualTo(Intent.ACTION_VIEW);
  }

  @Test
  public void create_url() {
    AnnotatorModel.ClassificationResult classificationResult =
        createClassificationResult(TextClassifier.TYPE_URL);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText= */ false,
            /* referenceTime= */ null,
            classificationResult);

    assertThat(intents).hasSize(1);
    assertThat(intents.get(0).intent.getAction()).isEqualTo(Intent.ACTION_VIEW);
  }

  @Test
  public void create_datetime_eventInFuture() {
    Instant referenceTime = Instant.now();
    Instant eventTime = referenceTime.plusSeconds(3600L);
    DatetimeResult datetimeResult =
        new DatetimeResult(eventTime.toEpochMilli(), DatetimeResult.GRANULARITY_SECOND);

    AnnotatorModel.ClassificationResult classificationResult =
        createClassificationResultWithDateTime(TextClassifier.TYPE_DATE_TIME, datetimeResult);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText= */ false,
            referenceTime,
            classificationResult);

    assertThat(intents).hasSize(2);
    assertThat(intents.get(0).intent.getAction()).isEqualTo(Intent.ACTION_VIEW);
    assertThat(intents.get(1).intent.getAction()).isEqualTo(Intent.ACTION_INSERT);
  }

  @Test
  public void create_datetime_eventInPast() {
    Instant referenceTime = Instant.now();
    Instant eventTime = referenceTime.minusSeconds(3600L);
    DatetimeResult datetimeResult =
        new DatetimeResult(eventTime.toEpochMilli(), DatetimeResult.GRANULARITY_SECOND);

    AnnotatorModel.ClassificationResult classificationResult =
        createClassificationResultWithDateTime(TextClassifier.TYPE_DATE_TIME, datetimeResult);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText= */ false,
            referenceTime,
            classificationResult);

    assertThat(intents).hasSize(1);
    assertThat(intents.get(0).intent.getAction()).isEqualTo(Intent.ACTION_VIEW);
  }

  @Test
  public void create_flight() {
    AnnotatorModel.ClassificationResult classificationResult =
        createClassificationResult(TextClassifier.TYPE_FLIGHT_NUMBER);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText= */ false,
            /* referenceTime= */ null,
            classificationResult);

    assertThat(intents).hasSize(1);
    assertThat(intents.get(0).intent.getAction()).isEqualTo(Intent.ACTION_WEB_SEARCH);
  }

  @Test
  public void create_typeDictionary() {
    AnnotatorModel.ClassificationResult classificationResult =
        createClassificationResult(TYPE_DICTIONARY);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText= */ false,
            /* referenceTime= */ null,
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
        createClassificationResult(TYPE_DICTIONARY);

    List<LabeledIntent> intents =
        legacyIntentClassificationFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText= */ true,
            /* referenceTime= */ null,
            classificationResult);

    assertThat(intents).hasSize(2);
    assertThat(intents.get(0).intent.getAction()).isEqualTo(Intent.ACTION_DEFINE);
    assertThat(intents.get(1).intent.getAction()).isEqualTo(Intent.ACTION_TRANSLATE);
  }

  private static ClassificationResult createClassificationResult(String collection) {
    return createClassificationResultWithDateTime(collection, /* datetimeResult= */ null);
  }

  private static ClassificationResult createClassificationResultWithDateTime(
      String collection, DatetimeResult datetimeResult) {
    return new AnnotatorModel.ClassificationResult(
        collection,
        1.0f,
        datetimeResult,
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
  }
}
