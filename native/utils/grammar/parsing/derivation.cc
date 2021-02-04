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

#include "utils/grammar/parsing/derivation.h"

#include <algorithm>

namespace libtextclassifier3::grammar {

bool Derivation::IsValid() const {
  bool result = true;
  Traverse(parse_tree, [&result](const ParseTree* node) {
    if (node->type != ParseTree::Type::kAssertion) {
      // Only validation if all checks so far passed.
      return result;
    }
    // Positive assertions are by definition fulfilled,
    // fail if the assertion is negative.
    if (static_cast<const AssertionNode*>(node)->negative) {
      result = false;
    }
    return result;
  });
  return result;
}

std::vector<Derivation> DeduplicateDerivations(
    const std::vector<Derivation>& derivations) {
  std::vector<Derivation> sorted_candidates = derivations;
  std::stable_sort(sorted_candidates.begin(), sorted_candidates.end(),
                   [](const Derivation& a, const Derivation& b) {
                     // Sort by id.
                     if (a.rule_id != b.rule_id) {
                       return a.rule_id < b.rule_id;
                     }

                     // Sort by increasing start.
                     if (a.parse_tree->codepoint_span.first !=
                         b.parse_tree->codepoint_span.first) {
                       return a.parse_tree->codepoint_span.first <
                              b.parse_tree->codepoint_span.first;
                     }

                     // Sort by decreasing end.
                     return a.parse_tree->codepoint_span.second >
                            b.parse_tree->codepoint_span.second;
                   });

  // Deduplicate by overlap.
  std::vector<Derivation> result;
  for (int i = 0; i < sorted_candidates.size(); i++) {
    const Derivation& candidate = sorted_candidates[i];
    bool eliminated = false;

    // Due to the sorting above, the candidate can only be completely
    // intersected by a match before it in the sorted order.
    for (int j = i - 1; j >= 0; j--) {
      if (sorted_candidates[j].rule_id != candidate.rule_id) {
        break;
      }
      if (sorted_candidates[j].parse_tree->codepoint_span.first <=
              candidate.parse_tree->codepoint_span.first &&
          sorted_candidates[j].parse_tree->codepoint_span.second >=
              candidate.parse_tree->codepoint_span.second) {
        eliminated = true;
        break;
      }
    }
    if (!eliminated) {
      result.push_back(candidate);
    }
  }
  return result;
}

std::vector<Derivation> ValidDeduplicatedDerivations(
    const std::vector<Derivation>& derivations) {
  std::vector<Derivation> result;
  for (const Derivation& derivation : DeduplicateDerivations(derivations)) {
    // Check that asserts are fulfilled.
    if (derivation.IsValid()) {
      result.push_back(derivation);
    }
  }
  return result;
}

}  // namespace libtextclassifier3::grammar
