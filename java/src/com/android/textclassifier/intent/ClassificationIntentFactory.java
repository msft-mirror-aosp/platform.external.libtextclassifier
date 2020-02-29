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
import android.content.Intent;
import com.android.textclassifier.R;
import com.android.textclassifier.common.intent.LabeledIntent;
import com.google.android.textclassifier.AnnotatorModel;
import com.google.common.collect.ImmutableList;
import java.time.Instant;
import java.util.List;
import javax.annotation.Nullable;

/** Generates intents from classification results. */
public interface ClassificationIntentFactory {

  /** Return a list of LabeledIntent from the classification result. */
  ImmutableList<LabeledIntent> create(
      Context context,
      String text,
      boolean foreignText,
      @Nullable Instant referenceTime,
      @Nullable AnnotatorModel.ClassificationResult classification);

  /** Inserts translate action to the list if it is a foreign text. */
  static void insertTranslateAction(List<LabeledIntent> actions, Context context, String text) {
    actions.add(
        new LabeledIntent(
            context.getString(R.string.tcs_translate),
            /* titleWithEntity */ null,
            context.getString(R.string.tcs_translate_desc),
            /* descriptionWithAppName */ null,
            new Intent(Intent.ACTION_TRANSLATE)
                // TODO: Probably better to introduce a "translate" scheme instead
                // of
                // using EXTRA_TEXT.
                .putExtra(Intent.EXTRA_TEXT, text),
            text.hashCode()));
  }
}
