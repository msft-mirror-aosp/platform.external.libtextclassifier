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
package com.android.textclassifier.intent;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Preconditions;

import com.android.textclassifier.TcLog;

import com.google.android.textclassifier.AnnotatorModel;
import com.google.android.textclassifier.RemoteActionTemplate;

import java.time.Instant;
import java.util.Collections;
import java.util.List;

/**
 * Creates intents based on {@link RemoteActionTemplate} objects for a ClassificationResult.
 *
 * @hide
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public final class TemplateClassificationIntentFactory implements ClassificationIntentFactory {
    private static final String TAG = "TemplateClassificationIntentFactory";
    private final TemplateIntentFactory mTemplateIntentFactory;
    private final ClassificationIntentFactory mFallback;

    public TemplateClassificationIntentFactory(
            TemplateIntentFactory templateIntentFactory, ClassificationIntentFactory fallback) {
        mTemplateIntentFactory = Preconditions.checkNotNull(templateIntentFactory);
        mFallback = Preconditions.checkNotNull(fallback);
    }

    /**
     * Returns a list of {@link LabeledIntent} that are constructed from the classification result.
     */
    @NonNull
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
                    "RemoteActionTemplate is missing, fallback to"
                            + " LegacyClassificationIntentFactory.");
            return mFallback.create(context, text, foreignText, referenceTime, classification);
        }
        final List<LabeledIntent> labeledIntents =
                mTemplateIntentFactory.create(remoteActionTemplates);
        if (foreignText) {
            ClassificationIntentFactory.insertTranslateAction(labeledIntents, context, text.trim());
        }
        return labeledIntents;
    }
}
