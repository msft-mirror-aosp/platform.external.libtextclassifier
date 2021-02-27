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

// Utilities for tests.

#ifndef LIBTEXTCLASSIFIER_UTILS_TOKENIZER_UTILS_H_
#define LIBTEXTCLASSIFIER_UTILS_TOKENIZER_UTILS_H_

#include <string>

#include "annotator/types.h"

namespace libtextclassifier3 {

// Returns a list of Tokens for a given input string, by tokenizing on space.
std::vector<Token> TokenizeOnSpace(const std::string& text);

// Returns a list of Tokens for a given input string, by tokenizing on the
// given set of delimiter codepoints.
// If create_tokens_for_non_space_delimiters is true, create tokens for
// delimiters which are not white spaces. For example "This, is" -> {"This",
// ",", "is"}.

std::vector<Token> TokenizeOnDelimiters(
    const std::string& text, const std::unordered_set<char32>& delimiters,
    bool create_tokens_for_non_space_delimiters = false);

}  // namespace  libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_TOKENIZER_UTILS_H_