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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_PARSING_DERIVATION_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_PARSING_DERIVATION_H_

#include <vector>

#include "utils/grammar/parsing/parse-tree.h"

namespace libtextclassifier3::grammar {

// A parse tree for a root rule.
struct Derivation {
  const ParseTree* parse_tree;
  int64 rule_id;

  // Checks that all assertions are fulfilled.
  bool IsValid() const;
};

// Deduplicates rule derivations by containing overlap.
// The grammar system can output multiple candidates for optional parts.
// For example if a rule has an optional suffix, we
// will get two rule derivations when the suffix is present: one with and one
// without the suffix. We therefore deduplicate by containing overlap, viz. from
// two candidates we keep the longer one if it completely contains the shorter.
std::vector<Derivation> DeduplicateDerivations(
    const std::vector<Derivation>& derivations);

// Deduplicates and validates rule derivations.
std::vector<Derivation> ValidDeduplicatedDerivations(
    const std::vector<Derivation>& derivations);

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_PARSING_DERIVATION_H_
