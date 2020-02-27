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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_GRAMMAR_ANNOTATOR_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_GRAMMAR_ANNOTATOR_H_

#include <vector>

#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/grammar/lexer.h"
#include "utils/i18n/locale.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

// Grammar backed annotator.
class GrammarAnnotator {
 public:
  enum class Callback : grammar::CallbackId {
    kRuleMatch = 1,
    kCapturingMatch = 2,
    kAssertionMatch = 3,
  };

  GrammarAnnotator(const UniLib* unilib, const GrammarModel* model);

  // Annotates a given tokenized text.
  bool Annotate(const std::vector<Locale>& locales,
                const std::vector<Token>& tokens,
                std::vector<AnnotatedSpan>* result) const;

 private:
  const UniLib* unilib_;
  const GrammarModel* model_;
  const grammar::Lexer lexer_;

  // Pre-parsed locales of the rules.
  const std::vector<std::vector<Locale>> rules_locales_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_GRAMMAR_ANNOTATOR_H_
