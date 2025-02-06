/*
 * Copyright (C) 2025 The Android Open Source Project
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

import static java.lang.String.format;

import android.icu.util.ULocale;
import android.os.Bundle;
import android.util.ArrayMap;
import android.view.textclassifier.TextLanguage;
import android.view.textclassifier.TextLinks;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.utils.TextClassifierUtils;

import com.google.common.annotations.VisibleForTesting;

import java.io.IOException;
import java.util.Collections;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Class with helper methods related to detecting OTP codes in a text.
 */
public class TextClassifierOtpHelper {
  private static final String TAG = TextClassifierOtpHelper.class.getSimpleName();

  private static final int PATTERN_FLAGS =
      Pattern.DOTALL | Pattern.CASE_INSENSITIVE | Pattern.MULTILINE;

  private static ThreadLocal<Matcher> compileToRegex(String pattern) {
    return ThreadLocal.withInitial(() -> Pattern.compile(pattern, PATTERN_FLAGS).matcher(""));
  }

  private static final float TC_THRESHOLD = 0.6f;

  private static final ArrayMap<String, ThreadLocal<Matcher>> EXTRA_LANG_OTP_REGEX =
      new ArrayMap<>();

  private static final ThreadLocal<Matcher> OTP_REGEX = compileToRegex(RegExStrings.ALL_OTP);

  /**
   * A combination of common false positives. These matches are expected to be longer than (or equal
   * in length to) otp matches
   */
  private static final ThreadLocal<Matcher> FALSE_POSITIVE_REGEX =
      compileToRegex(RegExStrings.FALSE_POSITIVE);

  /**
   * Creates a regular expression to match any of a series of individual words, case insensitive. It
   * also verifies the position of the word, relative to the OTP match
   */
  private static ThreadLocal<Matcher> createDictionaryRegex(String[] words) {
    StringBuilder regex = new StringBuilder("(");
    for (int i = 0; i < words.length; i++) {
      regex.append(findContextWordWithCode(words[i]));
      if (i != words.length - 1) {
        regex.append("|");
      }
    }
    regex.append(")");
    return compileToRegex(regex.toString());
  }

  /**
   * Creates a regular expression that will find a context word, if that word occurs in the sentence
   * preceding an OTP, or in the same sentence as an OTP (before or after). In both cases, the
   * context word must occur within 50 characters of the suspected OTP
   *
   * @param contextWord The context word we expect to find around the OTP match
   * @return A string representing a regular expression that will determine if we found a context
   *     word occurring before an otp match, or after it, but in the same sentence.
   */
  private static String findContextWordWithCode(String contextWord) {
    String boundedContext = "\\b" + contextWord + "\\b";
    // Asserts that we find the OTP code within 50 characters after the context word, with at
    // most one sentence punctuation between the OTP code and the context word (i.e. they are
    // in the same sentence, or the context word is in the previous sentence)
    String contextWordBeforeOtpInSameOrPreviousSentence =
        String.format("(%s(?=.{1,50}%s)[^.?!]*[.?!]?[^.?!]*%s)", boundedContext, RegExStrings.ALL_OTP, RegExStrings.ALL_OTP);
    // Asserts that we find the context word within 50 characters after the OTP code, with no
    // sentence punctuation between the OTP code and the context word (i.e. they are in the same
    // sentence)
    String contextWordAfterOtpSameSentence =
        String.format("(%s)[^.!?]{1,50}%s", RegExStrings.ALL_OTP, boundedContext);
    return String.format(
        "(%s|%s)", contextWordBeforeOtpInSameOrPreviousSentence, contextWordAfterOtpSameSentence);
  }

  static {
    EXTRA_LANG_OTP_REGEX.put(
        ULocale.ENGLISH.toLanguageTag(), createDictionaryRegex(RegExStrings.ENGLISH_CONTEXT_WORDS));
  }

  /**
   * Checks if the text might contain an OTP, if so, adds a link to the builder with type as OTP
   *
   * @param text    The text whose content should be checked for OTP
   * @param tcImpl  Instance of the TextClassifierImpl
   * @param builder TextLinks builder object to whom the OTP link to be added
   */
  public static void addOtpLink(@NonNull String text, @NonNull TextClassifierImpl tcImpl,
          @NonNull TextLinks.Builder builder) {
    if (!containsOtp(text, tcImpl)) {
      return;
    }
    final Map<String, Float> entityScores = Collections.singletonMap(TextClassifierUtils.TYPE_OTP,
            1f);
    builder.addLink(0, 0, entityScores, new Bundle());
  }

  /**
   * Checks if a string of text might contain an OTP, based on several regular expressions, and
   * potentially using a textClassifier to eliminate false positives
   *
   * @param text   The text whose content should be checked
   * @param tcImpl If non null, the provided TextClassifierImpl will be used to find the language
   *               of the text, and look for a language-specific regex for it.
   * @return True if we believe an OTP is in the message, false otherwise.
   */
  protected static boolean containsOtp(
          @NonNull String text,
          @NonNull TextClassifierImpl tcImpl) {
    if (!containsOtpLikePattern(text)) {
      return false;
    }

    ULocale language = getLanguageWithRegex(text, tcImpl);
    if (language == null) {
      return false;
    }
    return hasLanguageSpecificOtpWord(text, language.toLanguageTag());
  }

  /**
   * Checks if the given text contains a pattern resembling an OTP.
   *
   * <p>This method attempts to identify such patterns by matching against a regular expression.
   * Avoids false positives by checking for common patterns that might be mistaken for OTPs, such
   * as phone numbers or dates.</p>
   *
   * @param text The text to be checked.
   * @return {@code true} if the text contains an OTP-like pattern, {@code false} otherwise.
   */
  @VisibleForTesting
  protected static boolean containsOtpLikePattern(String text) {
    Set<String> otpMatches = getAllMatches(text, OTP_REGEX.get());
    if (otpMatches.isEmpty()) {
      return false;
    }
    Set<String> falsePositives = getAllMatches(text, FALSE_POSITIVE_REGEX.get());

    // This optional, but having this would help with performance
    // Example: "Your OTP code is 1234 and this is sent on 01-01-2001"
    // At this point -> otpMatches: [1234, 01-01-2001] falsePositives=[01-01-2001]
    // It filters "01-01-2001" in advance and continues to next checks with otpMatches: [1234]
    otpMatches.removeAll(falsePositives);

    // Following is to handle text like: "Your OTP can't be shared at this point, please call
    // (888) 888-8888"
    // otpMatches: [888-8888] falsePositives=[(888) 888-8888] final=[]
    for (String otpMatch : otpMatches) {
      boolean currentOtpIsFalsePositive = false;
      for (String falsePositive : falsePositives) {
        if (falsePositive.contains(otpMatch)) {
          currentOtpIsFalsePositive = true;
          break;
        }
      }
      if (!currentOtpIsFalsePositive) {
        return true;
      }
    }
    return false;
  }

  /**
   * Checks if the given text contains a language-specific word or phrase associated with OTPs.
   * This method uses regular expressions defined for specific languages to identify these words.
   *
   * @param text The text to check.
   * @param languageTag The language tag (e.g., "en", "es", "fr") for which to check.
   * @return {@code true} if the text contains a language-specific OTP word, {@code false} otherwise.
   *         Returns {@code false} if no language-specific regex is defined for the given tag.
   */
  @VisibleForTesting
  protected static boolean hasLanguageSpecificOtpWord(@NonNull String text, @NonNull String languageTag) {
    if (!EXTRA_LANG_OTP_REGEX.containsKey(languageTag)){
      return false;
    }
    Matcher languageSpecificMatcher = EXTRA_LANG_OTP_REGEX.get(languageTag).get();
    if (languageSpecificMatcher == null) {
      return false;
    }
    languageSpecificMatcher.reset(text);
    return languageSpecificMatcher.find();
  }

  private static Set<String> getAllMatches(String text, Matcher regex) {
    Set<String> matches = new HashSet<>();
    regex.reset(text);
    while (regex.find()) {
      matches.add(regex.group());
    }
    return matches;
  }

  // Tries to determine the language of the given text. Will return the language with the highest
  // confidence score that meets the minimum threshold, and has a language-specific regex, null
  // otherwise
  @Nullable
  private static ULocale getLanguageWithRegex(String text, @NonNull TextClassifierImpl tcImpl) {
    float highestConfidence = 0;
    ULocale highestConfidenceLocale = null;
    TextLanguage.Request langRequest = new TextLanguage.Request.Builder(text).build();
    TextLanguage lang;
    try {
      lang = tcImpl.detectLanguage(null, null, langRequest);
    } catch (IOException e) {
      TcLog.e(TAG, "Except detecting language", e);
      return null;
    }
    for (int i = 0; i < lang.getLocaleHypothesisCount(); i++) {
      ULocale locale = lang.getLocale(i);
      float confidence = lang.getConfidenceScore(locale);
      if (confidence >= TC_THRESHOLD
          && confidence >= highestConfidence
          && EXTRA_LANG_OTP_REGEX.containsKey(locale.toLanguageTag())) {
        highestConfidence = confidence;
        highestConfidenceLocale = locale;
      }
    }
    return highestConfidenceLocale;
  }

  private TextClassifierOtpHelper() {}

  private static class RegExStrings {
    /*
     * A regex matching a line start, open paren, arrow, colon (not proceeded by a digit), open square
     * bracket, equals sign, double or single quote, ideographic char, or a space that is not preceded
     * by a number. It will not consume the start char (meaning START won't be included in the matched
     * string)
     */
    private static final String START =
            "(^|(?<=((^|[^0-9])\\s)|[>(\"'=\\[\\p{IsIdeographic}]|[^0-9]:))";

    /*
     * A regex matching a line end, a space that is not followed by a number, an ideographic char, or
     * a period, close paren, close square bracket, single or double quote, exclamation point,
     * question mark, or comma. It will not consume the end char
     */
    private static final String END = "(?=\\s[^0-9]|$|\\p{IsIdeographic}|[.?!,)'\\]\"])";

    private static final String ALL_OTP;

    static {
      /* One single OTP char. A number or alphabetical char (that isn't also ideographic) */
      final String OTP_CHAR = "([0-9\\p{IsAlphabetic}&&[^\\p{IsIdeographic}]])";

      /* One OTP char, followed by an optional dash */
      final String OTP_CHAR_WITH_DASH = format("(%s-?)", OTP_CHAR);

      /*
       * Performs a lookahead to find a digit after 0 to 7 OTP_CHARs. This ensures that our potential
       * OTP code contains at least one number
       */
      final String FIND_DIGIT = format("(?=%s{0,7}\\d)", OTP_CHAR_WITH_DASH);

      /*
       * Matches between 5 and 8 otp chars, with dashes in between. Here, we are assuming an OTP code is
       * 5-8 characters long. The last char must not be followed by a dash
       */
      final String OTP_CHARS = format("(%s{4,7}%s)", OTP_CHAR_WITH_DASH, OTP_CHAR);

      /* A regex matching four digit numerical codes */
      final String FOUR_DIGITS = "(\\d{4})";

      final String FIVE_TO_EIGHT_ALPHANUM_AT_LEAST_ONE_NUM =
              format("(%s%s)", FIND_DIGIT, OTP_CHARS);

      /* A regex matching two pairs of 3 digits (ex "123 456") */
      final String SIX_DIGITS_WITH_SPACE = "(\\d{3}\\s\\d{3})";

      /*
       * Combining the regular expressions above, we get an OTP regex: 1. search for START, THEN 2.
       * match ONE of a. alphanumeric sequence, at least one number, length 5-8, with optional dashes b.
       * 4 numbers in a row c. pair of 3 digit codes separated by a space THEN 3. search for END Ex:
       * "6454", " 345 678.", "[YDT-456]"
       */
      ALL_OTP =
              format(
                      "%s(%s|%s|%s)%s",
                      START, FIVE_TO_EIGHT_ALPHANUM_AT_LEAST_ONE_NUM, FOUR_DIGITS,
                      SIX_DIGITS_WITH_SPACE, END);
    }

    private static final String FALSE_POSITIVE;

    static {
      /*
       * A Date regular expression. Looks for dates with the month, day, and year separated by dashes.
       * Handles one and two digit months and days, and four or two-digit years. It makes the following
       * assumptions: Dates and months will never be higher than 39 If a four digit year is used, the
       * leading digit will be 1 or 2
       */
      final String DATE_WITH_DASHES = "([0-3]?\\d-[0-3]?\\d-([12]\\d)?\\d\\d)";

      /*
       * matches a ten digit phone number, when the area code is separated by a space or dash. Supports
       * optional parentheses around the area code, and an optional dash or space in between the rest of
       * the numbers. This format registers as an otp match due to the space between the area code and
       * the rest, but shouldn't.
       */
      final String PHONE_WITH_SPACE = "(\\(?\\d{3}\\)?(-|\\s)?\\d{3}(-|\\s)?\\d{4})";

      /*
       * A combination of common false positives. These matches are expected to be longer than (or equal
       * in length to) otp matches.
       */
      FALSE_POSITIVE = format("%s(%s|%s)%s", START, DATE_WITH_DASHES, PHONE_WITH_SPACE, END);
    }

    /**
     * A list of regular expressions representing words found in an OTP context (non case sensitive)
     * Note: TAN is short for Transaction Authentication Number
     */
    private static final String[] ENGLISH_CONTEXT_WORDS =
            new String[] {
                    "pin",
                    "pass[-\\s]?(code|word)",
                    "TAN",
                    "otp",
                    "2fa",
                    "(two|2)[-\\s]?factor",
                    "log[-\\s]?in",
                    "auth(enticat(e|ion))?",
                    "code",
                    "secret",
                    "verif(y|ication)",
                    "one(\\s|-)?time",
                    "access",
                    "validat(e|ion)"
            };
  }
}
