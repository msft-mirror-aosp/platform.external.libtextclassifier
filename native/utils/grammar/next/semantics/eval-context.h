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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVAL_CONTEXT_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVAL_CONTEXT_H_

#include <unordered_map>

#include "utils/grammar/next/parsing/parse-tree.h"
#include "utils/grammar/next/semantics/value.h"
#include "utils/grammar/next/text-context.h"

namespace libtextclassifier3::grammar::next {

// Context for the evaluation of the semantic expression of a rule parse tree.
// This contains data about the evaluated constituents (named parts) of a rule
// and it's match.
struct EvalContext {
  // The input text.
  const TextContext* text_context = nullptr;

  // The syntactic parse tree that is begin evaluated.
  const ParseTree* parse_tree = nullptr;

  // A map of an id of a rule constituent (named part of a rule match) to it's
  // evaluated semantic value.
  std::unordered_map<int, const SemanticValue*> rule_constituents;
};

}  // namespace libtextclassifier3::grammar::next

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVAL_CONTEXT_H_
