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

#include "utils/grammar/rules-utils.h"

namespace libtextclassifier3::grammar {

std::vector<std::vector<Locale>> ParseRulesLocales(const RulesSet* rules) {
  if (rules == nullptr || rules->rules() == nullptr) {
    return {};
  }
  std::vector<std::vector<Locale>> locales(rules->rules()->size());
  for (int i = 0; i < rules->rules()->size(); i++) {
    const grammar::RulesSet_::Rules* rules_shard = rules->rules()->Get(i);
    if (rules_shard->locale() == nullptr) {
      continue;
    }
    for (const LanguageTag* tag : *rules_shard->locale()) {
      locales[i].push_back(Locale::FromLanguageTag(tag));
    }
  }
  return locales;
}

std::vector<RuleMatch> DeduplicateMatches(
    const std::vector<RuleMatch>& matches) {
  std::vector<RuleMatch> sorted_candidates = matches;
  std::stable_sort(
      sorted_candidates.begin(), sorted_candidates.end(),
      [](const RuleMatch& a, const RuleMatch& b) {
        // Sort by id.
        if (a.rule_id != b.rule_id) {
          return a.rule_id < b.rule_id;
        }

        // Sort by increasing start.
        if (a.match->codepoint_span.first != b.match->codepoint_span.first) {
          return a.match->codepoint_span.first < b.match->codepoint_span.first;
        }

        // Sort by decreasing end.
        return a.match->codepoint_span.second > b.match->codepoint_span.second;
      });

  // Deduplicate by overlap.
  std::vector<RuleMatch> result;
  for (int i = 0; i < sorted_candidates.size(); i++) {
    const RuleMatch& candidate = sorted_candidates[i];
    bool eliminated = false;

    // Due to the sorting above, the candidate can only be completely
    // intersected by a match before it in the sorted order.
    for (int j = i - 1; j >= 0; j--) {
      if (sorted_candidates[j].rule_id != candidate.rule_id) {
        break;
      }
      if (sorted_candidates[j].match->codepoint_span.first <=
              candidate.match->codepoint_span.first &&
          sorted_candidates[j].match->codepoint_span.second >=
              candidate.match->codepoint_span.second) {
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

bool VerifyAssertions(const Match* match) {
  bool result = true;
  grammar::Traverse(match, [&result](const Match* node) {
    if (node->type != Match::kAssertionMatch) {
      // Only validation if all checks so far passed.
      return result;
    }

    // Positive assertions are by definition fulfilled,
    // fail if the assertion is negative.
    if (static_cast<const AssertionMatch*>(node)->negative) {
      result = false;
    }
    return result;
  });
  return result;
}

std::unordered_map<uint16, const CapturingMatch*> GatherCapturingMatches(
    const Match* match) {
  // Gather active capturing matches.
  std::unordered_map<uint16, const CapturingMatch*> capturing_matches;
  grammar::Traverse(match, [&capturing_matches](const grammar::Match* node) {
    switch (node->type) {
      case Match::kCapturingMatch: {
        const CapturingMatch* capturing_match =
            static_cast<const CapturingMatch*>(node);
        capturing_matches[capturing_match->id] = capturing_match;
        return true;
      }

      // Don't traverse assertion nodes.
      case Match::kAssertionMatch: {
        return false;
      }

      default: {
        // Expand node.
        return true;
      }
    }
  });
  return capturing_matches;
}

}  // namespace libtextclassifier3::grammar
