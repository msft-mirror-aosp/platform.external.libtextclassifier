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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.view.textclassifier.TextClassifier;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.google.android.textclassifier.AnnotatorModel;
import com.google.android.textclassifier.RemoteActionTemplate;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class TemplateClassificationIntentFactoryTest {

  private static final String TEXT = "text";
  private static final String TITLE_WITHOUT_ENTITY = "Map";
  private static final String DESCRIPTION = "Opens in Maps";
  private static final String DESCRIPTION_WITH_APP_NAME = "Use %1$s to open Map";
  private static final String ACTION = Intent.ACTION_VIEW;

  @Mock private ClassificationIntentFactory fallback;
  private TemplateClassificationIntentFactory templateClassificationIntentFactory;

  @Before
  public void setup() {
    MockitoAnnotations.initMocks(this);
    templateClassificationIntentFactory =
        new TemplateClassificationIntentFactory(new TemplateIntentFactory(), fallback);
  }

  @Test
  public void create_foreignText() {
    AnnotatorModel.ClassificationResult classificationResult =
        new AnnotatorModel.ClassificationResult(
            TextClassifier.TYPE_ADDRESS,
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
            createRemoteActionTemplates(),
            0L,
            0L,
            0d);

    List<LabeledIntent> intents =
        templateClassificationIntentFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText */ true,
            null,
            classificationResult);

    assertThat(intents).hasSize(2);
    LabeledIntent labeledIntent = intents.get(0);
    assertThat(labeledIntent.titleWithoutEntity).isEqualTo(TITLE_WITHOUT_ENTITY);
    Intent intent = labeledIntent.intent;
    assertThat(intent.getAction()).isEqualTo(ACTION);

    labeledIntent = intents.get(1);
    intent = labeledIntent.intent;
    assertThat(intent.getAction()).isEqualTo(Intent.ACTION_TRANSLATE);
  }

  @Test
  public void create_notForeignText() {
    AnnotatorModel.ClassificationResult classificationResult =
        new AnnotatorModel.ClassificationResult(
            TextClassifier.TYPE_ADDRESS,
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
            createRemoteActionTemplates(),
            0L,
            0L,
            0d);

    List<LabeledIntent> intents =
        templateClassificationIntentFactory.create(
            ApplicationProvider.getApplicationContext(),
            TEXT,
            /* foreignText */ false,
            null,
            classificationResult);

    assertThat(intents).hasSize(1);
    LabeledIntent labeledIntent = intents.get(0);
    assertThat(labeledIntent.titleWithoutEntity).isEqualTo(TITLE_WITHOUT_ENTITY);
    Intent intent = labeledIntent.intent;
    assertThat(intent.getAction()).isEqualTo(ACTION);
  }

  @Test
  public void create_nullTemplate() {
    AnnotatorModel.ClassificationResult classificationResult =
        new AnnotatorModel.ClassificationResult(
            TextClassifier.TYPE_ADDRESS,
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

    templateClassificationIntentFactory.create(
        ApplicationProvider.getApplicationContext(),
        TEXT,
        /* foreignText */ false,
        null,
        classificationResult);

    verify(fallback)
        .create(
            same(ApplicationProvider.getApplicationContext()),
            eq(TEXT),
            eq(false),
            eq(null),
            same(classificationResult));
  }

  @Test
  public void create_emptyResult() {
    AnnotatorModel.ClassificationResult classificationResult =
        new AnnotatorModel.ClassificationResult(
            TextClassifier.TYPE_ADDRESS,
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
            new RemoteActionTemplate[0],
            0L,
            0L,
            0d);

    templateClassificationIntentFactory.create(
        ApplicationProvider.getApplicationContext(),
        TEXT,
        /* foreignText */ false,
        null,
        classificationResult);

    verify(fallback, never())
        .create(
            any(Context.class),
            eq(TEXT),
            eq(false),
            eq(null),
            any(AnnotatorModel.ClassificationResult.class));
  }

  private static RemoteActionTemplate[] createRemoteActionTemplates() {
    return new RemoteActionTemplate[] {
      new RemoteActionTemplate(
          TITLE_WITHOUT_ENTITY,
          null,
          DESCRIPTION,
          DESCRIPTION_WITH_APP_NAME,
          ACTION,
          null,
          null,
          null,
          null,
          null,
          null,
          null)
    };
  }
}
