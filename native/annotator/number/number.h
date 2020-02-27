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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_NUMBER_NUMBER_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_NUMBER_NUMBER_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/base/logging.h"
#include "utils/container/sorted-strings-table.h"
#include "utils/tokenizer.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {

// Annotator of numbers in text.
//
// Integer supported values are in range [-1 000 000 000, 1 000 000 000].
// Doble supposted values are in range [-999999999.999999999,
// 999999999.999999999].
class NumberAnnotator {
 public:
  explicit NumberAnnotator(const NumberAnnotatorOptions* options,
                           const UniLib* unilib)
      : options_(options),
        unilib_(unilib),
        tokenizer_(Tokenizer(TokenizationType_LETTER_DIGIT, unilib,
                             /*codepoint_ranges=*/{},
                             /*internal_tokenizer_codepoint_ranges=*/{},
                             /*split_on_script_change=*/false,
                             /*icu_preserve_whitespace_tokens=*/true)),
        percentage_pieces_string_(
            (options->percentage_pieces_string() == nullptr)
                ? StringPiece()
                : StringPiece(options->percentage_pieces_string()->data(),
                              options->percentage_pieces_string()->size())),
        percentage_pieces_offsets_(FlatbuffersIntVectorToStdVector(
            options->percentage_pieces_offsets())),
        percentage_suffixes_trie_(
            SortedStringsTable(/*num_pieces=*/percentage_pieces_offsets_.size(),
                               /*offsets=*/percentage_pieces_offsets_.data(),
                               /*pieces=*/percentage_pieces_string_)),
        max_number_of_digits_(options->max_number_of_digits()) {}

  // Classifies given text, and if it is a number, it passes the result in
  // 'classification_result' and returns true, otherwise returns false.
  bool ClassifyText(const UnicodeText& context, CodepointSpan selection_indices,
                    AnnotationUsecase annotation_usecase,
                    ClassificationResult* classification_result) const;

  // Finds all number instances in the input text. Returns true in any case.
  bool FindAll(const UnicodeText& context_unicode,
               AnnotationUsecase annotation_usecase,
               std::vector<AnnotatedSpan>* result) const;

 private:
  static std::vector<uint32> FlatbuffersIntVectorToStdVector(
      const flatbuffers::Vector<int32_t>* ints);

  // Get the length of the percent suffix at the specified index in the context.
  int GetPercentSuffixLength(const UnicodeText& context,
                             int index_codepoints) const;

  // Checks if the annotated numbers from the context represent percentages.
  // If yes, replaces the collection type and the annotation boundary in the
  // result.
  void FindPercentages(const UnicodeText& context,
                       std::vector<AnnotatedSpan>* result) const;

  // Checks if the tokens from in the interval [start_index-2, start_index] are
  // valid characters that can preced a number context.
  bool TokensAreValidStart(const std::vector<Token>& tokens,
                           const int start_index) const;

  // Checks if the tokens in the interval (..., prefix_end_index] are a valid
  // number prefix.
  bool TokensAreValidNumberPrefix(const std::vector<Token>& tokens,
                                  const int prefix_end_index) const;

  // Checks if the tokens from in the interval [ending_index, ending_index+2]
  // are valid characters that can follow a number context.
  bool TokensAreValidEnding(const std::vector<Token>& tokens,
                            const int ending_index) const;

  // Checks if the tokens in the interval [suffix_start_index, ...) are a valid
  // number suffix.
  bool TokensAreValidNumberSuffix(const std::vector<Token>& tokens,
                                  const int suffix_start_index) const;

  // Checks if the given text represents a number (either int or double).
  bool TryParseNumber(const UnicodeText& token_text, const bool is_negative,
                      int64* parsed_int_value,
                      double* parsed_double_value) const;

  // Checks if a word contains only CJT characters.
  bool IsCJTterm(UnicodeText::const_iterator token_begin_it,
                 const int token_length) const;

  const NumberAnnotatorOptions* options_;
  const UniLib* unilib_;
  const Tokenizer tokenizer_;
  const StringPiece percentage_pieces_string_;
  const std::vector<uint32> percentage_pieces_offsets_;
  const SortedStringsTable percentage_suffixes_trie_;
  const int max_number_of_digits_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_NUMBER_NUMBER_H_
