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

import android.content.Context;
import com.android.textclassifier.TcLog;
import com.google.android.textclassifier.AnnotatorModel;
import com.google.android.textclassifier.RemoteActionTemplate;
import com.google.common.base.Preconditions;
import java.time.Instant;
import java.util.Collections;
import java.util.List;
import javax.annotation.Nullable;

/**
 * Creates intents based on {@link RemoteActionTemplate} objects for a ClassificationResult.
 *
 * @hide
 */
public final class TemplateClassificationIntentFactory implements ClassificationIntentFactory {
  private static final String TAG = "TemplateIntentFactory";
  private final TemplateIntentFactory templateIntentFactory;
  private final ClassificationIntentFactory fallback;

  public TemplateClassificationIntentFactory(
      TemplateIntentFactory templateIntentFactory, ClassificationIntentFactory fallback) {
    this.templateIntentFactory = Preconditions.checkNotNull(templateIntentFactory);
    this.fallback = Preconditions.checkNotNull(fallback);
  }

  /**
   * Returns a list of {@link LabeledIntent} that are constructed from the classification result.
   */
  @Override
  public List<LabeledIntent> create(
      Context context,
      String text,
      boolean foreignText,
      @Nullable Instant referenceTime,
      @Nullable AnnotatorModel.ClassificationResult classification) {
    if (classification == null) {
      return Collections.emptyList();
    }
    RemoteActionTemplate[] remoteActionTemplates = classification.getRemoteActionTemplates();
    if (remoteActionTemplates == null) {
      // RemoteActionTemplate is missing, fallback.
      TcLog.w(
          TAG,
          "RemoteActionTemplate is missing, fallback to" + " LegacyClassificationIntentFactory.");
      return fallback.create(context, text, foreignText, referenceTime, classification);
    }
    final List<LabeledIntent> labeledIntents = templateIntentFactory.create(remoteActionTemplates);
    if (foreignText) {
      ClassificationIntentFactory.insertTranslateAction(labeledIntents, context, text.trim());
    }
    return labeledIntents;
  }
}
