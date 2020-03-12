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

#include "annotator/number/number.h"

#include <climits>
#include <cstdlib>
#include <string>

#include "annotator/collections.h"
#include "annotator/types.h"
#include "utils/base/logging.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {

bool NumberAnnotator::ClassifyText(
    const UnicodeText& context, CodepointSpan selection_indices,
    AnnotationUsecase annotation_usecase,
    ClassificationResult* classification_result) const {
  TC3_CHECK(classification_result != nullptr);

  const UnicodeText substring_selected = UnicodeText::Substring(
      context, selection_indices.first, selection_indices.second);

  std::vector<AnnotatedSpan> results;
  if (!FindAll(substring_selected, annotation_usecase, &results)) {
    return false;
  }

  for (const AnnotatedSpan& result : results) {
    if (result.classification.empty()) {
      continue;
    }

    // We make sure that the result span is equal to the stripped selection span
    // to avoid validating cases like "23 asdf 3.14 pct asdf". FindAll will
    // anyway only find valid numbers and percentages and a given selection with
    // more than two tokens won't pass this check.
    if (result.span.first + selection_indices.first ==
            selection_indices.first &&
        result.span.second + selection_indices.first ==
            selection_indices.second) {
      *classification_result = result.classification[0];
      return true;
    }
  }
  return false;
}

bool NumberAnnotator::IsCJTterm(UnicodeText::const_iterator token_begin_it,
                                const int token_length) const {
  auto token_end_it = token_begin_it;
  std::advance(token_end_it, token_length);
  for (auto char_it = token_begin_it; char_it < token_end_it; ++char_it) {
    if (!unilib_->IsCJTletter(*char_it)) {
      return false;
    }
  }
  return true;
}

bool NumberAnnotator::TokensAreValidStart(const std::vector<Token>& tokens,
                                          const int start_index) const {
  if (start_index < 0 || tokens[start_index].is_whitespace) {
    return true;
  }
  return false;
}

bool NumberAnnotator::TokensAreValidNumberPrefix(
    const std::vector<Token>& tokens, const int prefix_end_index) const {
  if (TokensAreValidStart(tokens, prefix_end_index)) {
    return true;
  }

  auto prefix_begin_it =
      UTF8ToUnicodeText(tokens[prefix_end_index].value, /*do_copy=*/false)
          .begin();
  const int token_length =
      tokens[prefix_end_index].end - tokens[prefix_end_index].start;
  if (token_length == 1 && unilib_->IsOpeningBracket(*prefix_begin_it) &&
      TokensAreValidStart(tokens, prefix_end_index - 1)) {
    return true;
  }
  if (token_length == 1 && unilib_->IsNumberSign(*prefix_begin_it) &&
      TokensAreValidStart(tokens, prefix_end_index - 1)) {
    return true;
  }
  if (token_length == 1 && unilib_->IsSlash(*prefix_begin_it) &&
      prefix_end_index >= 1 &&
      TokensAreValidStart(tokens, prefix_end_index - 2)) {
    int64 int_val;
    double double_val;
    return TryParseNumber(UTF8ToUnicodeText(tokens[prefix_end_index - 1].value,
                                            /*do_copy=*/false),
                          false, &int_val, &double_val);
  }
  if (IsCJTterm(prefix_begin_it, token_length)) {
    return true;
  }

  return false;
}

bool NumberAnnotator::TokensAreValidEnding(const std::vector<Token>& tokens,
                                           const int ending_index) const {
  if (ending_index >= tokens.size() || tokens[ending_index].is_whitespace) {
    return true;
  }

  auto ending_begin_it =
      UTF8ToUnicodeText(tokens[ending_index].value, /*do_copy=*/false).begin();
  if (ending_index == tokens.size() - 1 &&
      tokens[ending_index].end - tokens[ending_index].start == 1 &&
      unilib_->IsPunctuation(*ending_begin_it)) {
    return true;
  }
  if (ending_index < tokens.size() - 1 &&
      tokens[ending_index].end - tokens[ending_index].start == 1 &&
      unilib_->IsPunctuation(*ending_begin_it) &&
      tokens[ending_index + 1].is_whitespace) {
    return true;
  }

  return false;
}

bool NumberAnnotator::TokensAreValidNumberSuffix(
    const std::vector<Token>& tokens, const int suffix_start_index) const {
  if (TokensAreValidEnding(tokens, suffix_start_index)) {
    return true;
  }

  auto suffix_begin_it =
      UTF8ToUnicodeText(tokens[suffix_start_index].value, /*do_copy=*/false)
          .begin();

  if (GetPercentSuffixLength(UTF8ToUnicodeText(tokens[suffix_start_index].value,
                                               /*do_copy=*/false),
                             0) > 0 &&
      TokensAreValidEnding(tokens, suffix_start_index + 1)) {
    return true;
  }

  const int token_length =
      tokens[suffix_start_index].end - tokens[suffix_start_index].start;
  if (token_length == 1 && unilib_->IsSlash(*suffix_begin_it) &&
      suffix_start_index <= tokens.size() - 2 &&
      TokensAreValidEnding(tokens, suffix_start_index + 2)) {
    int64 int_val;
    double double_val;
    return TryParseNumber(
        UTF8ToUnicodeText(tokens[suffix_start_index + 1].value,
                          /*do_copy=*/false),
        false, &int_val, &double_val);
  }
  if (IsCJTterm(suffix_begin_it, token_length)) {
    return true;
  }

  return false;
}

bool NumberAnnotator::TryParseNumber(const UnicodeText& token_text,
                                     const bool is_negative,
                                     int64* parsed_int_value,
                                     double* parsed_double_value) const {
  if (token_text.ToUTF8String().size() >= max_number_of_digits_) {
    return false;
  }
  const bool is_double = unilib_->ParseDouble(token_text, parsed_double_value);
  if (!is_double) {
    return false;
  }
  *parsed_int_value = std::trunc(*parsed_double_value);
  if (is_negative) {
    *parsed_int_value *= -1;
    *parsed_double_value *= -1;
  }

  return true;
}

bool NumberAnnotator::FindAll(const UnicodeText& context,
                              AnnotationUsecase annotation_usecase,
                              std::vector<AnnotatedSpan>* result) const {
  if (!options_->enabled() || ((1 << annotation_usecase) &
                               options_->enabled_annotation_usecases()) == 0) {
    return true;
  }

  const std::vector<Token> tokens = tokenizer_.Tokenize(context);
  for (int i = 0; i < tokens.size(); ++i) {
    const Token token = tokens[i];
    if (tokens[i].value.empty() ||
        !unilib_->IsDigit(
            *UTF8ToUnicodeText(tokens[i].value, /*do_copy=*/false).begin())) {
      continue;
    }

    const UnicodeText token_text =
        UTF8ToUnicodeText(token.value, /*do_copy=*/false);
    int64 parsed_int_value;
    double parsed_double_value;
    bool is_negative =
        (i > 0) &&
        unilib_->IsMinus(
            *UTF8ToUnicodeText(tokens[i - 1].value, /*do_copy=*/false).begin());
    if (!TryParseNumber(token_text, is_negative, &parsed_int_value,
                        &parsed_double_value)) {
      continue;
    }
    if (!TokensAreValidNumberPrefix(tokens, is_negative ? i - 2 : i - 1) ||
        !TokensAreValidNumberSuffix(tokens, i + 1)) {
      continue;
    }

    const bool has_decimal = !(parsed_int_value == parsed_double_value);

    ClassificationResult classification{Collections::Number(),
                                        options_->score()};
    classification.numeric_value = parsed_int_value;
    classification.numeric_double_value = parsed_double_value;
    classification.priority_score =
        has_decimal ? options_->float_number_priority_score()
                    : options_->priority_score();

    AnnotatedSpan annotated_span;
    annotated_span.span = {is_negative ? token.start - 1 : token.start,
                           token.end};
    annotated_span.classification.push_back(classification);
    result->push_back(annotated_span);
  }

  if (options_->enable_percentage()) {
    FindPercentages(context, result);
  }

  return true;
}

std::vector<uint32> NumberAnnotator::FlatbuffersIntVectorToStdVector(
    const flatbuffers::Vector<int32_t>* ints) {
  if (ints == nullptr) {
    return {};
  }
  return {ints->begin(), ints->end()};
}

int NumberAnnotator::GetPercentSuffixLength(const UnicodeText& context,
                                            int index_codepoints) const {
  if (index_codepoints >= context.size_codepoints()) {
    return -1;
  }
  auto context_it = context.begin();
  std::advance(context_it, index_codepoints);
  const StringPiece suffix_context(
      context_it.utf8_data(),
      std::distance(context_it.utf8_data(), context.end().utf8_data()));
  StringSet::Match match;
  percentage_suffixes_trie_.LongestPrefixMatch(suffix_context, &match);

  if (match.match_length == -1) {
    return match.match_length;
  } else {
    return UTF8ToUnicodeText(context_it.utf8_data(), match.match_length,
                             /*do_copy=*/false)
        .size_codepoints();
  }
}

void NumberAnnotator::FindPercentages(
    const UnicodeText& context, std::vector<AnnotatedSpan>* result) const {
  std::vector<AnnotatedSpan> percentage_annotations;
  const int initial_result_size = result->size();
  for (int i = 0; i < initial_result_size; ++i) {
    AnnotatedSpan annotated_span = (*result)[i];
    if (annotated_span.classification.empty() ||
        annotated_span.classification[0].collection != Collections::Number()) {
      continue;
    }

    const int match_length =
        GetPercentSuffixLength(context, annotated_span.span.second);
    if (match_length > 0) {
      annotated_span.span = {annotated_span.span.first,
                             annotated_span.span.second + match_length};
      annotated_span.classification[0].collection = Collections::Percentage();
      annotated_span.classification[0].priority_score =
          options_->percentage_priority_score();
      result->push_back(annotated_span);
    }
  }
}

}  // namespace libtextclassifier3
