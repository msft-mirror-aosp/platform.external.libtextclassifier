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

#include "utils/grammar/utils/rules.h"

#include <set>

#include "utils/strings/append.h"

namespace libtextclassifier3::grammar {
namespace {

// Returns whether a nonterminal is a pre-defined one.
bool IsPredefinedNonterminal(const std::string& nonterminal_name) {
  if (nonterminal_name == kStartNonterm || nonterminal_name == kEndNonterm ||
      nonterminal_name == kTokenNonterm || nonterminal_name == kDigitsNonterm) {
    return true;
  }
  for (int digits = 1; digits <= kMaxNDigitsNontermLength; digits++) {
    if (nonterminal_name == strings::StringPrintf(kNDigitsNonterm, digits)) {
      return true;
    }
  }
  return false;
}

// Gets an assigned Nonterm for a nonterminal or kUnassignedNonterm if not yet
// assigned.
Nonterm GetAssignedIdForNonterminal(
    const int nonterminal, const std::unordered_map<int, Nonterm>& assignment) {
  const auto it = assignment.find(nonterminal);
  if (it == assignment.end()) {
    return kUnassignedNonterm;
  }
  return it->second;
}

// Checks whether all the nonterminals in the rhs of a rule have already been
// assigned Nonterm values.
bool IsRhsAssigned(const Rules::Rule& rule,
                   const std::unordered_map<int, Nonterm>& nonterminals) {
  for (const Rules::RhsElement& element : rule.rhs) {
    // Terminals are always considered assigned, check only for non-terminals.
    if (element.is_terminal) {
      continue;
    }
    if (GetAssignedIdForNonterminal(element.nonterminal, nonterminals) ==
        kUnassignedNonterm) {
      return false;
    }
  }
  return true;
}

// Lowers a single high-level rule down into the intermediate representation.
void LowerRule(const int lhs_index, const Rules::Rule& rule,
               std::unordered_map<int, Nonterm>* nonterminals, Ir* ir) {
  // Special case for terminal rules.
  if (rule.rhs.size() == 1 && rule.rhs.front().is_terminal) {
    (*nonterminals)[lhs_index] =
        ir->Add(Ir::Lhs{GetAssignedIdForNonterminal(lhs_index, *nonterminals),
                        /*callback=*/{rule.callback, rule.callback_param},
                        /*preconditions=*/{rule.max_whitespace_gap}},
                rule.rhs.front().terminal, rule.case_sensitive, rule.shard);
    return;
  }

  // Nonterminal rules.
  std::vector<Nonterm> rhs_nonterms;
  for (const Rules::RhsElement& element : rule.rhs) {
    if (element.is_terminal) {
      rhs_nonterms.push_back(ir->Add(Ir::Lhs{kUnassignedNonterm},
                                     element.terminal, rule.case_sensitive,
                                     rule.shard));
    } else {
      Nonterm nonterminal_id =
          GetAssignedIdForNonterminal(element.nonterminal, *nonterminals);
      TC3_CHECK_NE(nonterminal_id, kUnassignedNonterm);
      rhs_nonterms.push_back(nonterminal_id);
    }
  }
  (*nonterminals)[lhs_index] =
      ir->Add(Ir::Lhs{GetAssignedIdForNonterminal(lhs_index, *nonterminals),
                      /*callback=*/{rule.callback, rule.callback_param},
                      /*preconditions=*/{rule.max_whitespace_gap}},
              rhs_nonterms, rule.shard);
}

}  // namespace

int Rules::AddNonterminal(StringPiece nonterminal_name) {
  const std::string key = nonterminal_name.ToString();
  auto it = nonterminal_names_.find(key);
  if (it != nonterminal_names_.end()) {
    return it->second;
  }
  const int index = nonterminals_.size();
  nonterminals_.push_back(NontermInfo{nonterminal_name.ToString()});
  nonterminal_names_.insert(it, {key, index});
  return index;
}

// Note: For k optional components this creates 2^k rules, but it would be
// possible to be smarter about this and only use 2k rules instead.
// However that might be slower as it requires an extra rule firing at match
// time for every omitted optional element.
void Rules::ExpandOptionals(
    const int lhs, const std::vector<RhsElement>& rhs,
    const CallbackId callback, const int64 callback_param,
    const int8 max_whitespace_gap, const bool case_sensitive, const int shard,
    std::vector<int>::const_iterator optional_element_indices,
    std::vector<int>::const_iterator optional_element_indices_end,
    std::vector<bool>* omit_these) {
  if (optional_element_indices == optional_element_indices_end) {
    // Nothing is optional, so just generate a rule.
    Rule r;
    for (uint32 i = 0; i < rhs.size(); i++) {
      if (!omit_these->at(i)) {
        r.rhs.push_back(rhs[i]);
      }
    }
    r.callback = callback;
    r.callback_param = callback_param;
    r.max_whitespace_gap = max_whitespace_gap;
    r.case_sensitive = case_sensitive;
    r.shard = shard;
    nonterminals_[lhs].rules.push_back(rules_.size());
    rules_.push_back(r);
    return;
  }

  const int next_optional_part = *optional_element_indices;
  ++optional_element_indices;

  // Recursive call 1: The optional part is omitted.
  (*omit_these)[next_optional_part] = true;
  ExpandOptionals(lhs, rhs, callback, callback_param, max_whitespace_gap,
                  case_sensitive, shard, optional_element_indices,
                  optional_element_indices_end, omit_these);

  // Recursive call 2: The optional part is required.
  (*omit_these)[next_optional_part] = false;
  ExpandOptionals(lhs, rhs, callback, callback_param, max_whitespace_gap,
                  case_sensitive, shard, optional_element_indices,
                  optional_element_indices_end, omit_these);
}

void Rules::Add(StringPiece lhs, const std::vector<std::string>& rhs,
                const CallbackId callback, const int64 callback_param,
                const int8 max_whitespace_gap, const bool case_sensitive,
                const int shard) {
  TC3_CHECK(!rhs.empty()) << "Rhs cannot be empty (Lhs=" << lhs << ")";
  TC3_CHECK(!IsPredefinedNonterminal(lhs.ToString()));

  std::vector<RhsElement> rhs_elements;
  std::vector<int> optional_element_indices;
  for (StringPiece rhs_component : rhs) {
    // Check whether this component is optional.
    if (rhs_component[rhs_component.size() - 1] == '?') {
      optional_element_indices.push_back(rhs_elements.size());
      rhs_component.RemoveSuffix(1);
    }

    // Check whether this component is a non-terminal.
    if (rhs_component[0] == '<' &&
        rhs_component[rhs_component.size() - 1] == '>') {
      rhs_elements.push_back(RhsElement(AddNonterminal(rhs_component)));
    } else {
      // A terminal.
      // Sanity check for common typos -- '<' or '>' in a terminal.
      TC3_CHECK_EQ(rhs_component.find('<'), std::string::npos)
          << "Rhs terminal `" << rhs_component
          << "` contains an angle bracket.";
      TC3_CHECK_EQ(rhs_component.find('>'), std::string::npos)
          << "Rhs terminal `" << rhs_component
          << "` contains an angle bracket.";
      TC3_CHECK_EQ(rhs_component.find('?'), std::string::npos)
          << "Rhs terminal `" << rhs_component << "` contains a question mark.";
      rhs_elements.push_back(RhsElement(rhs_component.ToString()));
    }
  }

  TC3_CHECK_LT(optional_element_indices.size(), rhs_elements.size())
      << "Rhs must contain at least one non-optional element.";

  std::vector<bool> omit_these(rhs_elements.size(), false);
  ExpandOptionals(AddNonterminal(lhs), rhs_elements, callback, callback_param,
                  max_whitespace_gap, case_sensitive, shard,
                  optional_element_indices.begin(),
                  optional_element_indices.end(), &omit_these);
}

Ir Rules::Finalize() const {
  Ir rules(filters_, num_shards_);
  std::unordered_map<int, Nonterm> nonterminal_ids;

  // Pending rules to process.
  std::set<std::pair<int, int>> scheduled_rules;

  // Define all used predefined nonterminals.
  for (const auto it : nonterminal_names_) {
    if (IsPredefinedNonterminal(it.first)) {
      nonterminal_ids[it.second] = rules.AddUnshareableNonterminal(it.first);
    }
  }

  // Assign (unmergeable) Nonterm values to any nonterminals that have
  // multiple rules or that have a filter callback on some rule.
  for (int i = 0; i < nonterminals_.size(); i++) {
    const NontermInfo& nonterminal = nonterminals_[i];
    bool unmergeable = (nonterminal.rules.size() > 1);
    for (const int rule_index : nonterminal.rules) {
      const Rule& rule = rules_[rule_index];

      // Schedule rule.
      scheduled_rules.insert({i, rule_index});

      if (rule.callback != kNoCallback &&
          filters_.find(rule.callback) != filters_.end()) {
        unmergeable = true;
      }
    }
    if (unmergeable) {
      // Define unique nonterminal id.
      nonterminal_ids[i] = rules.AddUnshareableNonterminal(nonterminal.name);
    } else {
      nonterminal_ids[i] = rules.AddNonterminal(nonterminal.name);
    }
  }

  // Now, keep adding eligible rules (rules whose rhs is completely assigned)
  // until we can't make any more progress.
  // Note: The following code is quadratic in the worst case.
  // This seems fine as this will only run as part of the compilation of the
  // grammar rules during model assembly.
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto nt_and_rule = scheduled_rules.begin();
         nt_and_rule != scheduled_rules.end();) {
      const Rule& rule = rules_[nt_and_rule->second];
      if (IsRhsAssigned(rule, nonterminal_ids)) {
        // Compile the rule.
        LowerRule(/*lhs_index=*/nt_and_rule->first, rule, &nonterminal_ids,
                  &rules);
        scheduled_rules.erase(
            nt_and_rule++);  // Iterator is advanced before erase.
        changed = true;
        break;
      } else {
        nt_and_rule++;
      }
    }
  }
  TC3_CHECK(scheduled_rules.empty());
  return rules;
}

}  // namespace libtextclassifier3::grammar
