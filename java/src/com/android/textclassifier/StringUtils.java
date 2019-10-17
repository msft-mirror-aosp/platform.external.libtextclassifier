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

package com.android.textclassifier;

import androidx.annotation.GuardedBy;
import com.google.common.base.Preconditions;
import java.text.BreakIterator;

/**
 * Utility functions for TextClassifier methods.
 *
 * <ul>
 *   <li>Provides validation of input parameters to TextClassifier methods
 * </ul>
 *
 * Intended to be used only for TextClassifier purposes.
 */
public final class StringUtils {
  private static final String TAG = "StringUtils";

  @GuardedBy("WORD_ITERATOR")
  private static final BreakIterator WORD_ITERATOR = BreakIterator.getWordInstance();

  /**
   * Returns the substring of {@code text} that contains at least text from index {@code start}
   * <i>(inclusive)</i> to index {@code end} <i><(exclusive)/i> with the goal of returning text that
   * is at least {@code minimumLength}. If {@code text} is not long enough, this will return {@code
   * text}. This method returns text at word boundaries.
   *
   * @param text the source text
   * @param start the start index of text that must be included
   * @param end the end index of text that must be included
   * @param minimumLength minimum length of text to return if {@code text} is long enough
   */
  public static String getSubString(String text, int start, int end, int minimumLength) {
    Preconditions.checkArgument(start >= 0);
    Preconditions.checkArgument(end <= text.length());
    Preconditions.checkArgument(start <= end);

    if (text.length() < minimumLength) {
      return text;
    }

    final int length = end - start;
    if (length >= minimumLength) {
      return text.substring(start, end);
    }

    final int offset = (minimumLength - length) / 2;
    int iterStart = Math.max(0, Math.min(start - offset, text.length() - minimumLength));
    int iterEnd = Math.min(text.length(), iterStart + minimumLength);

    synchronized (WORD_ITERATOR) {
      WORD_ITERATOR.setText(text);
      iterStart =
          WORD_ITERATOR.isBoundary(iterStart)
              ? iterStart
              : Math.max(0, WORD_ITERATOR.preceding(iterStart));
      iterEnd =
          WORD_ITERATOR.isBoundary(iterEnd)
              ? iterEnd
              : Math.max(iterEnd, WORD_ITERATOR.following(iterEnd));
      WORD_ITERATOR.setText("");
      return text.substring(iterStart, iterEnd);
    }
  }

  private StringUtils() {}
}
