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
import android.text.TextUtils;
import com.google.common.base.Preconditions;
import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.Optional;
import java.util.StringJoiner;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import javax.annotation.Nullable;

/** Provide utils to generate and parse the result id. */
public final class ResultIdUtils {
  private static final String CLASSIFIER_ID = "androidtc";
  private static final String SEPARATOR_MODEL_NAME = ";";
  private static final String SEPARATOR_LOCALES = ",";
  private static final Pattern EXTRACT_MODEL_NAME_FROM_RESULT_ID =
      Pattern.compile("^[^|]*\\|([^|]*)\\|[^|]*$");

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
    return createId(ImmutableList.of(new ModelInfo(modelVersion, modelLocales)), hash);
  }

  /** Creates a string id that may be used to identify a TextClassifier result. */
  public static String createId(List<ModelInfo> modelInfos, int hash) {
    Preconditions.checkNotNull(modelInfos);
    final StringJoiner modelJoiner = new StringJoiner(SEPARATOR_MODEL_NAME);
    for (ModelInfo modelInfo : modelInfos) {
      modelJoiner.add(modelInfo.toModelName());
    }
    return String.format(Locale.US, "%s|%s|%d", CLASSIFIER_ID, modelJoiner, hash);
  }

  /** Returns the first model name encoded in the signature. */
  static String getModelName(@Nullable String signature) {
    return Optional.ofNullable(signature)
        .flatMap(s -> getModelNames(s).stream().findFirst())
        .orElse("");
  }

  /** Returns all the model names encoded in the signature. */
  static ImmutableList<String> getModelNames(@Nullable String signature) {
    if (TextUtils.isEmpty(signature)) {
      return ImmutableList.of();
    }
    Matcher matcher = EXTRACT_MODEL_NAME_FROM_RESULT_ID.matcher(signature);
    if (!matcher.find()) {
      return ImmutableList.of();
    }
    return ImmutableList.copyOf(Splitter.on(SEPARATOR_MODEL_NAME).splitToList(matcher.group(1)));
  }

  private ResultIdUtils() {}

  /** Model information of a model file. */
  public static class ModelInfo {
    private final int version;
    private final ImmutableList<Locale> locales;

    public ModelInfo(int version, List<Locale> locales) {
      this.version = version;
      this.locales = ImmutableList.copyOf(locales);
    }

    /** Returns a string representation of the model info. */
    private String toModelName() {
      final StringJoiner localesJoiner = new StringJoiner(SEPARATOR_LOCALES);
      for (Locale locale : locales) {
        localesJoiner.add(locale.toLanguageTag());
      }
      return String.format(Locale.US, "%s_v%d", localesJoiner, version);
    }
  }
}
