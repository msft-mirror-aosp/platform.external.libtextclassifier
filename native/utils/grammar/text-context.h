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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_TEXT_CONTEXT_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_TEXT_CONTEXT_H_

#include <vector>

#include "annotator/types.h"
#include "utils/i18n/locale.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3::grammar {

// Input to the parser.
struct TextContext {
  UnicodeText text;
  std::vector<Token> tokens;
  std::vector<Locale> locales;
  std::vector<AnnotatedSpan> annotations;
};

};  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_TEXT_CONTEXT_H_
