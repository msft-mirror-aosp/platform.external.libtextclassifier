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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_UTILS_RULES_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_UTILS_RULES_H_

#include <unordered_map>
#include <vector>

#include "utils/grammar/types.h"
#include "utils/grammar/utils/ir.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3::grammar {

// Utility functions for pre-processing, creating and testing context free
// grammars.
//
// All rules for a grammar will be collected in a rules object.
//
//    Rules r;
//    CallbackId date_output_callback = 1;
//    CallbackId day_filter_callback = 2;  r.DefineFilter(day_filter_callback);
//    CallbackId year_filter_callback = 3; r.DefineFilter(year_filter_callback);
//    r.Add("<date>", {"<monthname>", "<day>", <year>"},
//          date_output_callback);
//    r.Add("<monthname>", {"January"});
//    ...
//    r.Add("<monthname>", {"December"});
//    r.Add("<day>", {"<string_of_digits>"}, day_filter_callback);
//    r.Add("<year>", {"<string_of_digits>"}, year_filter_callback);
//
// The Add() method adds a rule with a given lhs, rhs, and (optionally)
// callback.  The rhs is just a list of terminals and nonterminals.  Anything
// surrounded in angle brackets is considered a nonterminal.  A "?" can follow
// any element of the RHS, like this:
//
//    r.Add("<date>", {"<monthname>", "<day>?", ",?", "<year>"});
//
// This indicates that the <day> and "," parts of the rhs are optional.
// (This is just notational shorthand for adding a bunch of rules.)
//
// Once you're done adding rules and callbacks to the Rules object,
// call r.Finalize() on it. This lowers the rule set into an internal
// representation.
class Rules {
 public:
  explicit Rules(const int num_shards = 1) : num_shards_(num_shards) {}

  // Represents one item in a right-hand side, a single terminal or nonterminal.
  struct RhsElement {
    RhsElement() {}
    explicit RhsElement(const std::string& terminal, const bool is_optional)
        : is_terminal(true), terminal(terminal), is_optional(is_optional) {}
    explicit RhsElement(const int nonterminal, const bool is_optional)
        : is_terminal(false),
          nonterminal(nonterminal),
          is_optional(is_optional) {}
    bool is_terminal;
    std::string terminal;
    int nonterminal;
    bool is_optional;
  };

  // Represents the right-hand side, and possibly callback, of one rule.
  struct Rule {
    std::vector<RhsElement> rhs;
    CallbackId callback = kNoCallback;
    int64 callback_param = 0;
    int8 max_whitespace_gap = -1;
    bool case_sensitive = false;
    int shard = 0;
  };

  struct NontermInfo {
    // The name of the non-terminal, if defined.
    std::string name;

    // Rules that have this non-terminal as the lhs.
    std::vector<int> rules;
  };

  // Adds a rule `lhs ::= rhs` with the given callback id and parameter.
  // Note: Nonterminal names are in angle brackets and cannot contain
  // whitespace. The `rhs` is a list of components, each of which is either:
  //  * A nonterminal name (in angle brackets)
  //  * A terminal
  // optionally followed by a `?` which indicates that the component is
  // optional. The `rhs` must contain at least one non-optional component.
  void Add(StringPiece lhs, const std::vector<std::string>& rhs,
           const CallbackId callback = kNoCallback,
           const int64 callback_param = 0, int8 max_whitespace_gap = -1,
           bool case_sensitive = false, int shard = 0);

  // Adds a rule `lhs ::= rhs` with the given callback id and parameter.
  // The `rhs` must contain at least one non-optional component.
  void Add(int lhs, const std::vector<RhsElement>& rhs,
           CallbackId callback = kNoCallback, int64 callback_param = 0,
           int8 max_whitespace_gap = -1, bool case_sensitive = false,
           int shard = 0);

  // Adds a rule `lhs ::= rhs` with exclusion.
  // The rule only matches, if `excluded_nonterminal` doesn't match the same
  // span.
  void AddWithExclusion(StringPiece lhs, const std::vector<std::string>& rhs,
                        StringPiece excluded_nonterminal,
                        int8 max_whitespace_gap = -1,
                        bool case_sensitive = false, int shard = 0);

  // Adds an assertion callback.
  void AddAssertion(StringPiece lhs, const std::vector<std::string>& rhs,
                    bool negative = true, int8 max_whitespace_gap = -1,
                    bool case_sensitive = false, int shard = 0);

  // Adds a mapping callback.
  void AddValueMapping(StringPiece lhs, const std::vector<std::string>& rhs,
                       int64 value, int8 max_whitespace_gap = -1,
                       bool case_sensitive = false, int shard = 0);

  // Creates a nonterminal with the given name, if one doesn't already exist.
  int AddNonterminal(StringPiece nonterminal_name);

  // Creates a new nonterminal.
  int AddNewNonterminal();

  // Defines a new filter id.
  void DefineFilter(const CallbackId filter_id) { filters_.insert(filter_id); }

  // Lowers the rule set into the intermediate representation.
  // Treats nonterminals given by the argument `predefined_nonterminals` as
  // defined externally. This allows to define rules that are dependent on
  // non-terminals produced by e.g. existing text annotations and that will be
  // fed to the matcher by the lexer.
  Ir Finalize(const std::set<std::string>& predefined_nonterminals = {}) const;

 private:
  void ExpandOptionals(
      int lhs, const std::vector<RhsElement>& rhs, CallbackId callback,
      int64 callback_param, int8 max_whitespace_gap, bool case_sensitive,
      int shard, std::vector<int>::const_iterator optional_element_indices,
      std::vector<int>::const_iterator optional_element_indices_end,
      std::vector<bool>* omit_these);

  const int num_shards_;

  // Non-terminal to id map.
  std::unordered_map<std::string, int> nonterminal_names_;
  std::vector<NontermInfo> nonterminals_;

  // Rules.
  std::vector<Rule> rules_;

  // Ids of callbacks that should be treated as filters.
  std::unordered_set<CallbackId> filters_;
};

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_UTILS_RULES_H_
