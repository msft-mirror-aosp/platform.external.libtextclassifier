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

// Unit tests for the lexer.

#include "utils/grammar/lexer.h"

#include <string>
#include <vector>

#include "utils/grammar/callback-delegate.h"
#include "utils/grammar/matcher.h"
#include "utils/grammar/rules_generated.h"
#include "utils/grammar/utils/ir.h"
#include "utils/tokenizer.h"
#include "utils/utf8/unilib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3::grammar {
namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::Value;

// Superclass of all tests here.
class LexerTest : public testing::Test {
 protected:
  LexerTest()
      : INIT_UNILIB_FOR_TESTING(unilib_),
        tokenizer_(TokenizationType_ICU, &unilib_,
                   /*codepoint_ranges=*/{},
                   /*internal_tokenizer_codepoint_ranges=*/{},
                   /*split_on_script_change=*/false,
                   /*icu_preserve_whitespace_tokens=*/true),
        lexer_(unilib_) {}

  // Creates a grammar just checking the specified terminals.
  std::string GrammarForTerminals(const std::vector<std::string>& terminals) {
    const CallbackId callback = 1;
    Ir ir;
    for (const std::string& terminal : terminals) {
      ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, terminal);
    }
    return ir.SerializeAsFlatbuffer();
  }

  UniLib unilib_;
  Tokenizer tokenizer_;
  Lexer lexer_;
};

struct TestMatchResult {
  int begin;
  int end;
  std::string terminal;
  std::string nonterminal;
};

MATCHER_P3(IsTerminal, begin, end, terminal, "") {
  return Value(arg.begin, begin) && Value(arg.end, end) &&
         Value(arg.terminal, terminal);
}

MATCHER_P3(IsNonterminal, begin, end, name, "") {
  return Value(arg.begin, begin) && Value(arg.end, end) &&
         Value(arg.nonterminal, name);
}

// This is a simple callback for testing purposes.
class TestCallbackDelegate : public CallbackDelegate {
 public:
  explicit TestCallbackDelegate(
      const RulesSet_::DebugInformation* debug_information)
      : debug_information_(debug_information) {}

  void MatchFound(const Match* match, const CallbackId, const int64,
                  Matcher*) override {
    TestMatchResult result;
    result.begin = match->match_offset;
    result.end = match->codepoint_span.second;
    if (match->IsTerminalRule()) {
      result.terminal = match->terminal;
    } else if (match->IsUnaryRule()) {
      // We use callbacks on unary rules to attach a callback to a
      // predefined lhs.
      result.nonterminal = GetNonterminalName(match->unary_rule_rhs()->lhs);
    } else {
      result.nonterminal = GetNonterminalName(match->lhs);
    }
    log_.push_back(result);
  }

  void Clear() { log_.clear(); }

  const std::vector<TestMatchResult>& log() const { return log_; }

 private:
  std::string GetNonterminalName(const Nonterm nonterminal) const {
    if (const RulesSet_::DebugInformation_::NonterminalNamesEntry* entry =
            debug_information_->nonterminal_names()->LookupByKey(nonterminal)) {
      return entry->value()->str();
    }
    // Unnamed Nonterm.
    return "()";
  }

  const RulesSet_::DebugInformation* debug_information_;
  std::vector<TestMatchResult> log_;
};

TEST_F(LexerTest, HandlesSimpleLetters) {
  std::string rules_buffer = GrammarForTerminals({"a", "is", "this", "word"});
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(tokenizer_.Tokenize("This is a word"), &matcher);

  EXPECT_THAT(test_logger.log().size(), Eq(4));
}

TEST_F(LexerTest, HandlesConcatedLettersAndDigit) {
  std::string rules_buffer =
      GrammarForTerminals({"1234", "4321", "a", "cde", "this"});
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(tokenizer_.Tokenize("1234This a4321cde"), &matcher);

  EXPECT_THAT(test_logger.log().size(), Eq(5));
}

TEST_F(LexerTest, HandlesPunctuation) {
  std::string rules_buffer = GrammarForTerminals({"10", "18", "2014", "/"});
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(tokenizer_.Tokenize("10/18/2014"), &matcher);

  EXPECT_THAT(test_logger.log().size(), Eq(5));
}

TEST_F(LexerTest, HandlesUTF8Punctuation) {
  std::string rules_buffer =
      GrammarForTerminals({"电话", "：", "0871", "—", "6857", "（", "曹"});
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(tokenizer_.Tokenize("电话：0871—6857（曹"), &matcher);

  EXPECT_THAT(test_logger.log().size(), Eq(7));
}

TEST_F(LexerTest, HandlesMixedPunctuation) {
  std::string rules_buffer =
      GrammarForTerminals({"电话", "：", "0871", "—", "6857", "（", "曹"});
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(tokenizer_.Tokenize("电话 ：0871—6857（曹"), &matcher);

  EXPECT_THAT(test_logger.log().size(), Eq(7));
}

// Tests that the tokenizer adds the correct tokens, including <digits>, to
// the Matcher.
TEST_F(LexerTest, CorrectTokenOutputWithDigits) {
  const CallbackId callback = 1;
  Ir ir;
  const Nonterm digits = ir.AddUnshareableNonterminal("<digits>");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, digits);
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "the");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "quick");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "brown");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "fox");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "2345");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "88");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, ".");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "<");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "&");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "\xE2\x80\x94");
  std::string rules_buffer =
      ir.SerializeAsFlatbuffer(/*include_debug_information=*/true);
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(
      tokenizer_.Tokenize("The.qUIck\n brown2345fox88 \xE2\x80\x94 the"),
      &matcher);

  EXPECT_THAT(
      test_logger.log(),
      ElementsAre(IsTerminal(0, 3, "the"), IsTerminal(3, 4, "."),
                  IsTerminal(4, 9, "quick"), IsTerminal(9, 16, "brown"),

                  // Lexer automatically creates a digits nonterminal.
                  IsTerminal(16, 20, "2345"), IsNonterminal(16, 20, "<digits>"),

                  IsTerminal(20, 23, "fox"),

                  // Lexer automatically creates a digits nonterminal.
                  IsTerminal(23, 25, "88"), IsNonterminal(23, 25, "<digits>"),

                  IsTerminal(25, 27, "\xE2\x80\x94"),
                  IsTerminal(27, 31, "the")));
}

// Tests that the tokenizer adds the correct tokens, including the
// special <token> tokens, to the Matcher.
TEST_F(LexerTest, CorrectTokenOutputWithGenericTokens) {
  const CallbackId callback = 1;
  Ir ir;
  const Nonterm token = ir.AddUnshareableNonterminal("<token>");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, token);
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "the");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "quick");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "brown");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "fox");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "2345");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "88");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, ".");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "<");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "&");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "\xE2\x80\x94");
  std::string rules_buffer =
      ir.SerializeAsFlatbuffer(/*include_debug_information=*/true);
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(
      tokenizer_.Tokenize("The.qUIck\n brown2345fox88 \xE2\x80\x94 the"),
      &matcher);

  EXPECT_THAT(
      test_logger.log(),
      // Lexer will create a <token> nonterminal for each token.
      ElementsAre(IsTerminal(0, 3, "the"), IsNonterminal(0, 3, "<token>"),
                  IsTerminal(3, 4, "."), IsNonterminal(3, 4, "<token>"),
                  IsTerminal(4, 9, "quick"), IsNonterminal(4, 9, "<token>"),
                  IsTerminal(9, 16, "brown"), IsNonterminal(9, 16, "<token>"),
                  IsTerminal(16, 20, "2345"), IsNonterminal(16, 20, "<token>"),
                  IsTerminal(20, 23, "fox"), IsNonterminal(20, 23, "<token>"),
                  IsTerminal(23, 25, "88"), IsNonterminal(23, 25, "<token>"),
                  IsTerminal(25, 27, "\xE2\x80\x94"),
                  IsNonterminal(25, 27, "<token>"), IsTerminal(27, 31, "the"),
                  IsNonterminal(27, 31, "<token>")));
}

// Tests that the tokenizer adds the correct tokens, including <digits>, to
// the Matcher.  This test includes UTF8 letters.
TEST_F(LexerTest, CorrectTokenOutputWithDigitsAndUTF8) {
  const CallbackId callback = 1;
  Ir ir;
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}},
         ir.AddUnshareableNonterminal("<digits>"));

  const std::string em_dash = "\xE2\x80\x94";  // "Em dash" in UTF-8
  const std::string capital_i_acute = "\xC3\x8D";
  const std::string lower_i_acute = "\xC3\xAD";

  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "the");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, ".");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}},
         "quick" + lower_i_acute + lower_i_acute + lower_i_acute + "h");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, em_dash);
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, lower_i_acute + "h");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "22");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "brown");
  std::string rules_buffer =
      ir.SerializeAsFlatbuffer(/*include_debug_information=*/true);
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(tokenizer_.Tokenize("The.qUIck" + lower_i_acute +
                                     capital_i_acute + lower_i_acute + "h" +
                                     em_dash + capital_i_acute + "H22brown"),
                 &matcher);

  EXPECT_THAT(
      test_logger.log(),
      ElementsAre(IsTerminal(0, 3, "the"), IsTerminal(3, 4, "."),
                  IsTerminal(4, 13, "quickíííh"), IsTerminal(13, 14, "—"),
                  IsTerminal(14, 16, "íh"), IsTerminal(16, 18, "22"),
                  IsNonterminal(16, 18, "<digits>"),
                  IsTerminal(18, 23, "brown")));
}

// Tests that the tokenizer adds the correct tokens to the Matcher.
// For this test, there's no <digits> nonterminal in the Rules, and some
// tokens aren't in any grammar rules.
TEST_F(LexerTest, CorrectTokenOutputWithoutDigits) {
  const CallbackId callback = 1;
  Ir ir;
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "the");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "2345");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "88");
  std::string rules_buffer =
      ir.SerializeAsFlatbuffer(/*include_debug_information=*/true);
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(tokenizer_.Tokenize("The.qUIck\n brown2345fox88"), &matcher);

  EXPECT_THAT(test_logger.log(),
              ElementsAre(IsTerminal(0, 3, "the"), IsTerminal(16, 20, "2345"),
                          IsTerminal(23, 25, "88")));
}

// Tests that the tokenizer adds the correct <n_digits> tokens to the Matcher.
TEST_F(LexerTest, CorrectTokenOutputWithNDigits) {
  const CallbackId callback = 1;
  Ir ir;
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}},
         ir.AddUnshareableNonterminal("<digits>"));
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}},
         ir.AddUnshareableNonterminal("<2_digits>"));
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}},
         ir.AddUnshareableNonterminal("<4_digits>"));
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "the");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "2345");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "88");
  std::string rules_buffer =
      ir.SerializeAsFlatbuffer(/*include_debug_information=*/true);
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(tokenizer_.Tokenize("The.qUIck\n brown2345fox88"), &matcher);

  EXPECT_THAT(
      test_logger.log(),
      ElementsAre(IsTerminal(0, 3, "the"),
                  // Lexer should generate <digits> and <4_digits> for 2345.
                  IsTerminal(16, 20, "2345"), IsNonterminal(16, 20, "<digits>"),
                  IsNonterminal(16, 20, "<4_digits>"),

                  // Lexer should generate <digits> and <2_digits> for 88.
                  IsTerminal(23, 25, "88"), IsNonterminal(23, 25, "<digits>"),
                  IsNonterminal(23, 25, "<2_digits>")));
}

// Tests that the tokenizer splits "million+" into separate tokens.
TEST_F(LexerTest, CorrectTokenOutputWithPlusSigns) {
  const CallbackId callback = 1;
  Ir ir;
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "the");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "2345");
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}}, "+");
  const std::string lower_i_acute = "\xC3\xAD";
  ir.Add(Ir::Lhs{kUnassignedNonterm, {callback}},
         lower_i_acute + lower_i_acute);
  std::string rules_buffer =
      ir.SerializeAsFlatbuffer(/*include_debug_information=*/true);
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  lexer_.Process(tokenizer_.Tokenize("The+2345++the +" + lower_i_acute +
                                     lower_i_acute + "+"),
                 &matcher);

  EXPECT_THAT(test_logger.log(),
              ElementsAre(IsTerminal(0, 3, "the"), IsTerminal(3, 4, "+"),
                          IsTerminal(4, 8, "2345"), IsTerminal(8, 9, "+"),
                          IsTerminal(9, 10, "+"), IsTerminal(10, 13, "the"),
                          IsTerminal(13, 15, "+"), IsTerminal(15, 17, "íí"),
                          IsTerminal(17, 18, "+")));
}

// Tests that the tokenizer correctly uses the anchor tokens.
TEST_F(LexerTest, HandlesStartAnchor) {
  const CallbackId log_callback = 1;
  Ir ir;
  // <test> ::= <^> the test <$>
  ir.Add(Ir::Lhs{ir.AddNonterminal("<test>"), {log_callback}},
         std::vector<Nonterm>{
             ir.AddUnshareableNonterminal(kStartNonterm),
             ir.Add(Ir::Lhs{kUnassignedNonterm, {log_callback}}, "the"),
             ir.Add(Ir::Lhs{kUnassignedNonterm, {log_callback}}, "test"),
             ir.AddUnshareableNonterminal(kEndNonterm),
         });
  std::string rules_buffer =
      ir.SerializeAsFlatbuffer(/*include_debug_information=*/true);
  const RulesSet* rules = flatbuffers::GetRoot<RulesSet>(rules_buffer.data());
  TestCallbackDelegate test_logger(rules->debug_information());
  Matcher matcher(unilib_, rules, &test_logger);

  // Make sure the grammar recognizes "the test".
  lexer_.Process(tokenizer_.Tokenize("the test"), &matcher);
  EXPECT_THAT(test_logger.log(),
              // Expect logging of the two terminals and then matching of the
              // nonterminal.
              ElementsAre(IsTerminal(0, 3, "the"), IsTerminal(3, 8, "test"),
                          IsNonterminal(0, 8, "<test>")));

  // Make sure that only left anchored matches are propagated.
  test_logger.Clear();
  lexer_.Process(tokenizer_.Tokenize("the the test"), &matcher);  // NOTYPO
  EXPECT_THAT(test_logger.log(),
              // Expect that "<test>" nonterminal is not matched.
              ElementsAre(IsTerminal(0, 3, "the"), IsTerminal(3, 7, "the"),
                          IsTerminal(7, 12, "test")));

  // Make sure that only right anchored matches are propagated.
  test_logger.Clear();
  lexer_.Process(tokenizer_.Tokenize("the test test"), &matcher);
  EXPECT_THAT(test_logger.log(),
              // Expect that "<test>" nonterminal is not matched.
              ElementsAre(IsTerminal(0, 3, "the"), IsTerminal(3, 8, "test"),
                          IsTerminal(8, 13, "test")));
}

}  // namespace
}  // namespace libtextclassifier3::grammar
