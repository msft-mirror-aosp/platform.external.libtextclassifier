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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_PARSING_PARSER_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_PARSING_PARSER_H_

#include <vector>

#include "annotator/types.h"
#include "utils/grammar/lexer.h"
#include "utils/grammar/rules-utils.h"
#include "utils/grammar/rules_generated.h"
#include "utils/grammar/text-context.h"
#include "utils/i18n/locale.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3::grammar {

// Syntactic parsing pass.
// The parser validates and deduplicates candidates produced by the grammar
// matcher. It augments the parse trees with derivation information for semantic
// evaluation.
class Parser {
 public:
  // The grammar matcher callbacks handled by the parser.
  enum class Callback : CallbackId {
    // A match of a root rule.
    kRootRule = 1,
  };

  explicit Parser(const UniLib* unilib, const RulesSet* rules);

  // Parses an input text and returns the root rule derivations.
  std::vector<Derivation> Parse(const TextContext& input);

 private:
  const UniLib& unilib_;
  const RulesSet* rules_;
  const Lexer lexer_;

  // Pre-parsed locales of the rules.
  const std::vector<std::vector<Locale>> rules_locales_;
};

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_PARSING_PARSER_H_
