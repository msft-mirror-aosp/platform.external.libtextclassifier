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

#include "utils/grammar/matcher.h"

#include <iostream>
#include <string>
#include <vector>

#include "utils/grammar/callback-delegate.h"
#include "utils/grammar/rules_generated.h"
#include "utils/grammar/types.h"
#include "utils/grammar/utils/rules.h"
#include "utils/strings/append.h"
#include "utils/utf8/unilib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3::grammar {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Value;

struct TestMatchResult {
  int begin;
  int end;
  std::string terminal;
  std::string nonterminal;
  int callback_id;
  int callback_param;
};

MATCHER_P3(IsTerminal, begin, end, terminal, "") {
  return Value(arg.begin, begin) && Value(arg.end, end) &&
         Value(arg.terminal, terminal);
}

MATCHER_P4(IsTerminalWithCallback, begin, end, terminal, callback, "") {
  return (ExplainMatchResult(IsTerminal(begin, end, terminal), arg,
                             result_listener) &&
          Value(arg.callback_id, callback));
}

MATCHER_P3(IsNonterminal, begin, end, name, "") {
  return Value(arg.begin, begin) && Value(arg.end, end) &&
         Value(arg.nonterminal, name);
}

MATCHER_P4(IsNonterminalWithCallback, begin, end, name, callback, "") {
  return (ExplainMatchResult(IsNonterminal(begin, end, name), arg,
                             result_listener) &&
          Value(arg.callback_id, callback));
}

MATCHER_P5(IsNonterminalWithCallbackAndParam, begin, end, name, callback, param,
           "") {
  return (
      ExplainMatchResult(IsNonterminalWithCallback(begin, end, name, callback),
                         arg, result_listener) &&
      Value(arg.callback_param, param));
}

// Superclass of all tests.
class MatcherTest : public testing::Test {
 protected:
  MatcherTest() : INIT_UNILIB_FOR_TESTING(unilib_) {}

  UniLib unilib_;
};

// This is a simple delegate implementation for testing purposes.
// All it does is produce a record of all matches that were added.
class TestCallbackDelegate : public CallbackDelegate {
 public:
  explicit TestCallbackDelegate(
      const RulesSet_::DebugInformation* debug_information)
      : debug_information_(debug_information) {}

  void MatchFound(const Match* match, const CallbackId callback_id,
                  const int64 callback_param, Matcher*) override {
    TestMatchResult result;
    result.begin = match->codepoint_span.first;
    result.end = match->codepoint_span.second;
    result.callback_id = callback_id;
    result.callback_param = static_cast<int>(callback_param);
    result.nonterminal = GetNonterminalName(match->lhs);
    if (match->IsTerminalRule()) {
      result.terminal = match->terminal;
    }
    log_.push_back(result);
  }

  void ClearLog() { log_.clear(); }

  const std::vector<TestMatchResult> GetLogAndReset() {
    const auto result = log_;
    ClearLog();
    return result;
  }

 protected:
  std::string GetNonterminalName(const Nonterm nonterminal) const {
    if (const RulesSet_::DebugInformation_::NonterminalNamesEntry* entry =
            debug_information_->nonterminal_names()->LookupByKey(nonterminal)) {
      return entry->value()->str();
    }
    // Unnamed Nonterm.
    return "()";
  }

  std::vector<TestMatchResult> log_;
  const RulesSet_::DebugInformation* debug_information_;
};

TEST_F(MatcherTest, HandlesBasicOperations) {
  // Create an example grammar.
  Rules rules;
  const CallbackId callback = 1;
  rules.Add("<test>", {"the", "quick", "brown", "fox"}, callback);
  rules.Add("<action>", {"<test>"}, callback);
  const std::string rules_buffer = rules.Finalize().SerializeAsFlatbuffer(
      /*include_debug_information=*/true);
  const RulesSet* rules_set =
      flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules_set->debug_information());
  Matcher matcher(unilib_, rules_set, &test_logger);

  matcher.AddTerminal(0, 1, "the");
  matcher.AddTerminal(1, 2, "quick");
  matcher.AddTerminal(2, 3, "brown");
  matcher.AddTerminal(3, 4, "fox");

  EXPECT_THAT(test_logger.GetLogAndReset(),
              ElementsAre(IsNonterminal(0, 4, "<test>"),
                          IsNonterminal(0, 4, "<action>")));
}

std::string CreateTestGrammar() {
  // Create an example grammar.
  Rules rules;

  // Callbacks on terminal rules.
  rules.Add("<output_5>", {"quick"}, 6);
  rules.Add("<output_0>", {"the"}, 1);

  // Callbacks on non-terminal rules.
  rules.Add("<output_1>", {"the", "quick", "brown", "fox"}, 2);
  rules.Add("<output_2>", {"the", "quick"}, 3, static_cast<int64>(-1));
  rules.Add("<output_3>", {"brown", "fox"}, 4);
  // Now a complex thing: "the* brown fox".
  rules.Add("<thestarbrownfox>", {"brown", "fox"}, 5);
  rules.Add("<thestarbrownfox>", {"the", "<thestarbrownfox>"}, 5);

  return rules.Finalize().SerializeAsFlatbuffer(
      /*include_debug_information=*/true);
}

std::string CreateTestGrammarWithOptionalElements() {
  // Create an example grammar.
  Rules rules;
  rules.Add("<output_0>", {"a?", "b?", "c?", "d?", "e"}, 1);
  rules.Add("<output_1>", {"a", "b?", "c", "d?", "e"}, 2);
  rules.Add("<output_2>", {"a", "b?", "c", "d", "e?"}, 3);

  return rules.Finalize().SerializeAsFlatbuffer(
      /*include_debug_information=*/true);
}

Nonterm FindNontermForName(const RulesSet* rules,
                           const std::string& nonterminal_name) {
  for (const RulesSet_::DebugInformation_::NonterminalNamesEntry* entry :
       *rules->debug_information()->nonterminal_names()) {
    if (entry->value()->str() == nonterminal_name) {
      return entry->key();
    }
  }
  return kUnassignedNonterm;
}

TEST_F(MatcherTest, HandlesBasicOperationsWithCallbacks) {
  const std::string rules_buffer = CreateTestGrammar();
  const RulesSet* rules_set =
      flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules_set->debug_information());
  Matcher matcher(unilib_, rules_set, &test_logger);

  matcher.AddTerminal(0, 1, "the");
  EXPECT_THAT(test_logger.GetLogAndReset(),
              ElementsAre(IsTerminalWithCallback(/*begin=*/0, /*end=*/1, "the",
                                                 /*callback=*/1)));
  matcher.AddTerminal(1, 2, "quick");
  EXPECT_THAT(
      test_logger.GetLogAndReset(),
      ElementsAre(IsTerminalWithCallback(/*begin=*/1, /*end=*/2, "quick",
                                         /*callback=*/6),
                  IsNonterminalWithCallbackAndParam(
                      /*begin=*/0, /*end=*/2, "<output_2>",
                      /*callback=*/3, -1)));

  matcher.AddTerminal(2, 3, "brown");
  EXPECT_THAT(test_logger.GetLogAndReset(), IsEmpty());

  matcher.AddTerminal(3, 4, "fox");
  EXPECT_THAT(
      test_logger.GetLogAndReset(),
      ElementsAre(
          IsNonterminalWithCallback(/*begin=*/0, /*end=*/4, "<output_1>",
                                    /*callback=*/2),
          IsNonterminalWithCallback(/*begin=*/2, /*end=*/4, "<output_3>",
                                    /*callback=*/4),
          IsNonterminalWithCallback(/*begin=*/2, /*end=*/4, "<thestarbrownfox>",
                                    /*callback=*/5)));

  matcher.AddTerminal(3, 5, "fox");
  EXPECT_THAT(
      test_logger.GetLogAndReset(),
      ElementsAre(
          IsNonterminalWithCallback(/*begin=*/0, /*end=*/5, "<output_1>",
                                    /*callback=*/2),
          IsNonterminalWithCallback(/*begin=*/2, /*end=*/5, "<output_3>",
                                    /*callback=*/4),
          IsNonterminalWithCallback(/*begin=*/2, /*end=*/5, "<thestarbrownfox>",
                                    /*callback=*/5)));

  matcher.AddTerminal(4, 6, "fox");  // Not adjacent to "brown".
  EXPECT_THAT(test_logger.GetLogAndReset(), IsEmpty());
}

TEST_F(MatcherTest, HandlesRecursiveRules) {
  const std::string rules_buffer = CreateTestGrammar();
  const RulesSet* rules_set =
      flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules_set->debug_information());
  Matcher matcher(unilib_, rules_set, &test_logger);

  matcher.AddTerminal(0, 1, "the");
  matcher.AddTerminal(1, 2, "the");
  matcher.AddTerminal(2, 4, "the");
  matcher.AddTerminal(3, 4, "the");
  matcher.AddTerminal(4, 5, "brown");
  matcher.AddTerminal(5, 6, "fox");  // Generates 5 of <thestarbrownfox>

  EXPECT_THAT(
      test_logger.GetLogAndReset(),
      ElementsAre(IsTerminal(/*begin=*/0, /*end=*/1, "the"),
                  IsTerminal(/*begin=*/1, /*end=*/2, "the"),
                  IsTerminal(/*begin=*/2, /*end=*/4, "the"),
                  IsTerminal(/*begin=*/3, /*end=*/4, "the"),
                  IsNonterminal(/*begin=*/4, /*end=*/6, "<output_3>"),
                  IsNonterminal(/*begin=*/4, /*end=*/6, "<thestarbrownfox>"),
                  IsNonterminal(/*begin=*/3, /*end=*/6, "<thestarbrownfox>"),
                  IsNonterminal(/*begin=*/2, /*end=*/6, "<thestarbrownfox>"),
                  IsNonterminal(/*begin=*/1, /*end=*/6, "<thestarbrownfox>"),
                  IsNonterminal(/*begin=*/0, /*end=*/6, "<thestarbrownfox>")));
}

TEST_F(MatcherTest, HandlesManualAddMatchCalls) {
  const std::string rules_buffer = CreateTestGrammar();
  const RulesSet* rules_set =
      flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules_set->debug_information());
  Matcher matcher(unilib_, rules_set, &test_logger);

  // Test having the lexer call AddMatch() instead of AddTerminal()
  matcher.AddTerminal(-4, 37, "the");
  Match* match = matcher.AllocateMatch(sizeof(Match));
  match->codepoint_span = {37, 42};
  match->match_offset = 37;
  match->lhs = FindNontermForName(rules_set, "<thestarbrownfox>");
  matcher.AddMatch(match);

  EXPECT_THAT(test_logger.GetLogAndReset(),
              ElementsAre(IsTerminal(/*begin=*/-4, /*end=*/37, "the"),
                          IsNonterminal(/*begin=*/-4, /*end=*/42,
                                        "<thestarbrownfox>")));
}

TEST_F(MatcherTest, HandlesOptionalRuleElements) {
  const std::string rules_buffer = CreateTestGrammarWithOptionalElements();
  const RulesSet* rules_set =
      flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules_set->debug_information());
  Matcher matcher(unilib_, rules_set, &test_logger);

  // Run the matcher on "a b c d e".
  matcher.AddTerminal(0, 1, "a");
  matcher.AddTerminal(1, 2, "b");
  matcher.AddTerminal(2, 3, "c");
  matcher.AddTerminal(3, 4, "d");
  matcher.AddTerminal(4, 5, "e");

  EXPECT_THAT(test_logger.GetLogAndReset(),
              ElementsAre(IsNonterminal(/*begin=*/0, /*end=*/4, "<output_2>"),
                          IsTerminal(/*begin=*/4, /*end=*/5, "e"),
                          IsNonterminal(/*begin=*/0, /*end=*/5, "<output_0>"),
                          IsNonterminal(/*begin=*/0,
                                        /*end=*/5, "<output_1>"),
                          IsNonterminal(/*begin=*/0, /*end=*/5, "<output_2>"),
                          IsNonterminal(/*begin=*/1,
                                        /*end=*/5, "<output_0>"),
                          IsNonterminal(/*begin=*/2, /*end=*/5, "<output_0>"),
                          IsNonterminal(/*begin=*/3,
                                        /*end=*/5, "<output_0>")));
}

class FilterTestCallbackDelegate : public TestCallbackDelegate {
 public:
  FilterTestCallbackDelegate(
      const RulesSet_::DebugInformation* debug_information,
      const std::string& filter)
      : TestCallbackDelegate(debug_information), filter_(filter) {}

  void MatchFound(const Match* candidate, const CallbackId callback_id,
                  const int64 callback_param, Matcher* matcher) override {
    TestCallbackDelegate::MatchFound(candidate, callback_id, callback_param,
                                     matcher);
    // Filter callback.
    if (callback_id == 1) {
      if (candidate->IsTerminalRule() && filter_ != candidate->terminal) {
        return;
      } else {
        std::vector<const Match*> terminals = SelectTerminals(candidate);
        if (terminals.empty() || terminals.front()->terminal != filter_) {
          return;
        }
      }
      Match* match = matcher->AllocateAndInitMatch<Match>(*candidate);
      matcher->AddMatch(match);
      matcher->AddTerminal(match->codepoint_span, match->match_offset,
                           "faketerminal");
    }
  }

 protected:
  const std::string filter_;
};

std::string CreateExampleGrammarWithFilters() {
  // Create an example grammar.
  Rules rules;
  const CallbackId filter_callback = 1;
  rules.DefineFilter(filter_callback);

  rules.Add("<term_pass>", {"hello"}, filter_callback);
  rules.Add("<nonterm_pass>", {"hello", "there"}, filter_callback);
  rules.Add("<term_fail>", {"there"}, filter_callback);
  rules.Add("<nonterm_fail>", {"there", "world"}, filter_callback);

  // We use this to test whether AddTerminal() worked from inside a filter
  // callback.
  const CallbackId output_callback = 2;
  rules.Add("<output_faketerminal>", {"faketerminal"}, output_callback);

  // We use this to test whether AddMatch() worked from inside a filter
  // callback.
  rules.Add("<output_term_pass>", {"<term_pass>"}, output_callback);
  rules.Add("<output_nonterm_pass>", {"<nonterm_pass>"}, output_callback);
  rules.Add("<output_term_fail>", {"<term_fail>"}, output_callback);
  rules.Add("<output_nonterm_fail>", {"<term_nonterm_fail>"}, output_callback);

  // We use this to make sure rules with output callbacks are always adding
  // their lhs to the chart. This is to check for off-by-one errors in the
  // callback numbering, make sure we don't mistakenly treat the output
  // callback as a filter callback.
  rules.Add("<output>", {"<output_faketerminal>"}, output_callback);
  rules.Add("<output>", {"<output_term_pass>"}, output_callback);
  rules.Add("<output>", {"<output_nonterm_pass>"}, output_callback);

  return rules.Finalize().SerializeAsFlatbuffer(
      /*include_debug_information=*/true);
}

TEST_F(MatcherTest, HandlesTerminalFilters) {
  const std::string rules_buffer = CreateExampleGrammarWithFilters();
  const RulesSet* rules_set =
      flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  FilterTestCallbackDelegate test_logger(rules_set->debug_information(),
                                         "hello");
  Matcher matcher(unilib_, rules_set, &test_logger);
  matcher.AddTerminal(0, 1, "hello");

  EXPECT_THAT(
      test_logger.GetLogAndReset(),
      ElementsAre(
          // Bubbling up of:
          // "hello" -> "<term_pass>" -> "<output_term_pass>" -> "<output>"
          IsNonterminal(0, 1, "<term_pass>"),
          IsNonterminal(0, 1, "<output_term_pass>"),
          IsNonterminal(0, 1, "<output>"),

          // Bubbling up of:
          // "faketerminal" -> "<output_faketerminal>" -> "<output>"
          IsNonterminal(0, 1, "<output_faketerminal>"),
          IsNonterminal(0, 1, "<output>")));
}

TEST_F(MatcherTest, HandlesNonterminalFilters) {
  const std::string rules_buffer = CreateExampleGrammarWithFilters();
  const RulesSet* rules_set =
      flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  FilterTestCallbackDelegate test_logger(rules_set->debug_information(),
                                         "hello");
  Matcher matcher(unilib_, rules_set, &test_logger);

  matcher.AddTerminal(0, 1, "hello");
  test_logger.ClearLog();
  matcher.AddTerminal(1, 2, "there");

  EXPECT_THAT(test_logger.GetLogAndReset(),
              ElementsAre(
                  // "<term_fail>" is filtered, and doesn't bubble up.
                  IsNonterminal(1, 2, "<term_fail>"),

                  // <nonterm_pass> ::= hello there
                  IsNonterminal(0, 2, "<nonterm_pass>"),
                  IsNonterminal(0, 2, "<output_faketerminal>"),
                  IsNonterminal(0, 2, "<output>"),
                  IsNonterminal(0, 2, "<output_nonterm_pass>"),
                  IsNonterminal(0, 2, "<output>")));
}

TEST_F(MatcherTest, HandlesWhitespaceGapLimits) {
  Rules rules;
  rules.Add("<iata>", {"lx"});
  rules.Add("<iata>", {"aa"});
  // Require no whitespace between code and flight number.
  rules.Add("<flight_number>", {"<iata>", "<4_digits>"}, /*callback=*/1, 0,
            /*max_whitespace_gap=*/0);
  const std::string rules_buffer = rules.Finalize().SerializeAsFlatbuffer(
      /*include_debug_information=*/true);
  const RulesSet* rules_set =
      flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules_set->debug_information());
  Matcher matcher(unilib_, rules_set, &test_logger);

  // Check that the grammar triggers on LX1138.
  {
    matcher.AddTerminal(0, 2, "LX");
    matcher.AddMatch(matcher.AllocateAndInitMatch<Match>(
        rules_set->nonterminals()->n_digits_nt()->Get(4 - 1),
        CodepointSpan{2, 6}, /*match_offset=*/2));
    EXPECT_THAT(test_logger.GetLogAndReset(),
                ElementsAre(IsNonterminal(0, 6, "<flight_number>")));
  }

  // Check that the grammar doesn't trigger on LX 1138.
  {
    matcher.AddTerminal(6, 8, "LX");
    matcher.AddMatch(matcher.AllocateAndInitMatch<Match>(
        rules_set->nonterminals()->n_digits_nt()->Get(4 - 1),
        CodepointSpan{9, 13}, /*match_offset=*/8));
    EXPECT_THAT(test_logger.GetLogAndReset(), IsEmpty());
  }
}

TEST_F(MatcherTest, HandlesCaseSensitiveTerminals) {
  Rules rules;
  rules.Add("<iata>", {"LX"}, /*callback=*/kNoCallback, 0,
            /*max_whitespace_gap*/ -1, /*case_sensitive=*/true);
  rules.Add("<iata>", {"AA"}, /*callback=*/kNoCallback, 0,
            /*max_whitespace_gap*/ -1, /*case_sensitive=*/true);
  rules.Add("<iata>", {"dl"}, /*callback=*/kNoCallback, 0,
            /*max_whitespace_gap*/ -1, /*case_sensitive=*/false);
  // Require no whitespace between code and flight number.
  rules.Add("<flight_number>", {"<iata>", "<4_digits>"}, /*callback=*/1, 0,
            /*max_whitespace_gap=*/0);
  const std::string rules_buffer = rules.Finalize().SerializeAsFlatbuffer(
      /*include_debug_information=*/true);
  const RulesSet* rules_set =
      flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules_set->debug_information());
  Matcher matcher(unilib_, rules_set, &test_logger);

  // Check that the grammar triggers on LX1138.
  {
    matcher.AddTerminal(0, 2, "LX");
    matcher.AddMatch(matcher.AllocateAndInitMatch<Match>(
        rules_set->nonterminals()->n_digits_nt()->Get(4 - 1),
        CodepointSpan{2, 6}, /*match_offset=*/2));
    EXPECT_THAT(test_logger.GetLogAndReset(),
                ElementsAre(IsNonterminal(0, 6, "<flight_number>")));
  }

  // Check that the grammar doesn't trigger on lx1138.
  {
    matcher.AddTerminal(6, 8, "lx");
    matcher.AddMatch(matcher.AllocateAndInitMatch<Match>(
        rules_set->nonterminals()->n_digits_nt()->Get(4 - 1),
        CodepointSpan{8, 12}, /*match_offset=*/8));
    EXPECT_THAT(test_logger.GetLogAndReset(), IsEmpty());
  }

  // Check that the grammar does trigger on dl1138.
  {
    matcher.AddTerminal(12, 14, "dl");
    matcher.AddMatch(matcher.AllocateAndInitMatch<Match>(
        rules_set->nonterminals()->n_digits_nt()->Get(4 - 1),
        CodepointSpan{14, 18}, /*match_offset=*/14));
    EXPECT_THAT(test_logger.GetLogAndReset(),
                ElementsAre(IsNonterminal(12, 18, "<flight_number>")));
  }
}

}  // namespace
}  // namespace libtextclassifier3::grammar
