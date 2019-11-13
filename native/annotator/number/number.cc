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

  const CodepointSpan stripped_selection_indices =
      feature_processor_->StripBoundaryCodepoints(
          context, selection_indices, ignored_prefix_span_boundary_codepoints_,
          ignored_suffix_span_boundary_codepoints_);

  for (const AnnotatedSpan& result : results) {
    if (result.classification.empty()) {
      continue;
    }

    // We make sure that the result span is equal to the stripped selection span
    // to avoid validating cases like "23 asdf 3.14 pct asdf". FindAll will
    // anyway only find valid numbers and percentages and a given selection with
    // more than two tokens won't pass this check.
    if (result.span.first + selection_indices.first ==
            stripped_selection_indices.first &&
        result.span.second + selection_indices.first ==
            stripped_selection_indices.second) {
      *classification_result = result.classification[0];
      return true;
    }
  }
  return false;
}

bool NumberAnnotator::FindAll(const UnicodeText& context,
                              AnnotationUsecase annotation_usecase,
                              std::vector<AnnotatedSpan>* result) const {
  if (!options_->enabled() || ((1 << annotation_usecase) &
                               options_->enabled_annotation_usecases()) == 0) {
    return true;
  }

  const std::vector<Token> tokens = feature_processor_->Tokenize(context);
  for (const Token& token : tokens) {
    const UnicodeText token_text =
        UTF8ToUnicodeText(token.value, /*do_copy=*/false);
    int64 parsed_int_value;
    double parsed_double_value;
    bool has_decimal;
    int num_prefix_codepoints;
    int num_suffix_codepoints;
    if (ParseNumber(token_text, &parsed_int_value, &parsed_double_value,
                    &has_decimal, &num_prefix_codepoints,
                    &num_suffix_codepoints)) {
      ClassificationResult classification{Collections::Number(),
                                          options_->score()};
      classification.numeric_value = parsed_int_value;
      classification.numeric_double_value = parsed_double_value;
      classification.priority_score =
          has_decimal ? options_->float_number_priority_score()
                      : options_->priority_score();

      AnnotatedSpan annotated_span;
      annotated_span.span = {token.start + num_prefix_codepoints,
                             token.end - num_suffix_codepoints};
      annotated_span.classification.push_back(classification);
      result->push_back(annotated_span);
    }
  }

  if (options_->enable_percentage()) {
    FindPercentages(context, result);
  }

  return true;
}

std::unordered_set<int> NumberAnnotator::FlatbuffersIntVectorToSet(
    const flatbuffers::Vector<int32_t>* ints) {
  if (ints == nullptr) {
    return {};
  }
  return {ints->begin(), ints->end()};
}

std::vector<uint32> NumberAnnotator::FlatbuffersIntVectorToStdVector(
    const flatbuffers::Vector<int32_t>* ints) {
  if (ints == nullptr) {
    return {};
  }
  return {ints->begin(), ints->end()};
}

namespace {
bool ParseNextNumericCodepoint(int32 codepoint, int64* current_value) {
  if (*current_value > INT64_MAX / 10 - 10) {
    return false;
  }

  // NOTE: This currently just works with ASCII numbers.
  *current_value = *current_value * 10 + codepoint - '0';
  return true;
}

UnicodeText::const_iterator ConsumeAndParseNumber(
    const UnicodeText::const_iterator& it_begin,
    const UnicodeText::const_iterator& it_end, int64* int_result,
    double* double_result, bool* has_decimal) {
  *int_result = 0;
  *has_decimal = false;

  // See if there's a sign in the beginning of the number.
  int sign = 1;
  auto it = it_begin;
  while (it != it_end && (*it == '-' || *it == '+')) {
    if (*it == '-') {
      sign = -1;
    } else {
      sign = 1;
    }
    ++it;
  }

  enum class State {
    PARSING_WHOLE_PART = 1,
    PARSING_FLOATING_PART = 2,
    PARSING_DONE = 3,
  };
  State state = State::PARSING_WHOLE_PART;
  int64 decimal_result = 0;
  int64 decimal_result_denominator = 1;
  int number_digits = 0;
  while (it != it_end) {
    switch (state) {
      case State::PARSING_WHOLE_PART:
        if (*it >= '0' && *it <= '9') {
          if (!ParseNextNumericCodepoint(*it, int_result)) {
            return it_begin;
          }
        } else if (*it == '.' || *it == ',') {
          state = State::PARSING_FLOATING_PART;
        } else {
          state = State::PARSING_DONE;
        }
        break;
      case State::PARSING_FLOATING_PART:
        if (*it >= '0' && *it <= '9') {
          *has_decimal = true;
          if (!ParseNextNumericCodepoint(*it, &decimal_result)) {
            state = State::PARSING_DONE;
            break;
          }
          decimal_result_denominator *= 10;
        } else {
          state = State::PARSING_DONE;
        }
        break;
      case State::PARSING_DONE:
        break;
    }

    if (state == State::PARSING_DONE) {
      break;
    }
    ++number_digits;
    ++it;
  }

  if (number_digits == 0) {
    return it_begin;
  }

  *int_result *= sign;
  *double_result =
      *int_result + decimal_result * 1.0 / decimal_result_denominator;

  return it;
}
}  // namespace

bool NumberAnnotator::ParseNumber(const UnicodeText& text, int64* int_result,
                                  double* double_result, bool* has_decimal,
                                  int* num_prefix_codepoints,
                                  int* num_suffix_codepoints) const {
  TC3_CHECK(int_result != nullptr && double_result != nullptr &&
            num_prefix_codepoints != nullptr &&
            num_suffix_codepoints != nullptr);
  auto it = text.begin();
  auto it_end = text.end();

  // Strip boundary codepoints from both ends.
  const CodepointSpan original_span{0, text.size_codepoints()};
  const CodepointSpan stripped_span =
      feature_processor_->StripBoundaryCodepoints(
          text, original_span, ignored_prefix_span_boundary_codepoints_,
          ignored_suffix_span_boundary_codepoints_);

  const int num_stripped_end = (original_span.second - stripped_span.second);
  std::advance(it, stripped_span.first);
  std::advance(it_end, -num_stripped_end);

  // Consume prefix codepoints.
  *num_prefix_codepoints = stripped_span.first;
  while (it != it_end) {
    if (allowed_prefix_codepoints_.find(*it) ==
        allowed_prefix_codepoints_.end()) {
      break;
    }

    ++it;
    ++(*num_prefix_codepoints);
  }

  auto it_start = it;
  it =
      ConsumeAndParseNumber(it, it_end, int_result, double_result, has_decimal);
  if (it == it_start) {
    return false;
  }

  // Consume suffix codepoints.
  bool valid_suffix = true;
  *num_suffix_codepoints = 0;
  int ignored_suffix_codepoints = 0;
  while (it != it_end) {
    if (allowed_suffix_codepoints_.find(*it) !=
        allowed_suffix_codepoints_.end()) {
      // Keep track of allowed suffix codepoints.
      ++(*num_suffix_codepoints);
    } else if (ignored_suffix_span_boundary_codepoints_.find(*it) ==
               ignored_suffix_span_boundary_codepoints_.end()) {
      // There is a suffix codepoint but it's not part of the ignored list of
      // codepoints, fail the number parsing.
      // Note: We want to support cases like "13.", "34#", "123!" etc.
      valid_suffix = false;
      break;
    } else {
      ++ignored_suffix_codepoints;
    }

    ++it;
  }
  *num_suffix_codepoints += num_stripped_end;

  return valid_suffix;
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
  for (auto& res : *result) {
    if (res.classification.empty() ||
        res.classification[0].collection != Collections::Number()) {
      continue;
    }

    const int match_length = GetPercentSuffixLength(context, res.span.second);
    if (match_length > 0) {
      res.classification[0].collection = Collections::Percentage();
      res.classification[0].priority_score =
          options_->percentage_priority_score();
      res.span = {res.span.first, res.span.second + match_length};
    }
  }
}

}  // namespace libtextclassifier3
