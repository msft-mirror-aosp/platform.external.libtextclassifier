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

package com.android.textclassifier.common.statsd;

import android.content.Context;
import com.google.common.base.Preconditions;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.StringJoiner;
import javax.annotation.Nullable;

/** Provide utils to generate and parse the result id. */
public final class ResultIdUtils {
  private static final String CLASSIFIER_ID = "androidtc";

  /** Creates a string id that may be used to identify a TextClassifier result. */
  public static String createId(
      Context context,
      String text,
      int start,
      int end,
      int modelVersion,
      List<Locale> modelLocales) {
    Preconditions.checkNotNull(text);
    Preconditions.checkNotNull(context);
    Preconditions.checkNotNull(modelLocales);
    final int hash = Objects.hash(text, start, end, context.getPackageName());
    return createId(modelVersion, modelLocales, hash);
  }

  /** Creates a string id that may be used to identify a TextClassifier result. */
  public static String createId(int modelVersion, List<Locale> modelLocales, int hash) {
    final StringJoiner localesJoiner = new StringJoiner(",");
    for (Locale locale : modelLocales) {
      localesJoiner.add(locale.toLanguageTag());
    }
    final String modelName =
        String.format(Locale.US, "%s_v%d", localesJoiner.toString(), modelVersion);
    return String.format(Locale.US, "%s|%s|%d", CLASSIFIER_ID, modelName, hash);
  }

  static String getModelName(@Nullable String signature) {
    if (signature == null) {
      return "";
    }
    final int start = signature.indexOf("|") + 1;
    final int end = signature.indexOf("|", start);
    if (start >= 1 && end >= start) {
      return signature.substring(start, end);
    }
    return "";
  }

  private ResultIdUtils() {}
}
