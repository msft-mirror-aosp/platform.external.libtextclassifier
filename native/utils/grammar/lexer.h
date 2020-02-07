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

// This is a lexer that runs off the tokenizer and outputs the tokens to a
// grammar matcher. The tokens it forwards are the same as the ones produced
// by the tokenizer, but possibly further split and normalized (downcased).
// Examples:
//
//    - single character tokens for punctuation (e.g., AddTerminal("?"))
//
//    - a string of letters (e.g., "Foo" -- it calls AddTerminal() on "foo")
//
//    - a string of digits (e.g., AddTerminal("37"))
//
// In addition to the terminal tokens above, it also outputs certain
// special nonterminals:
//
//    - a <token> nonterminal, which it outputs in addition to the
//      regular AddTerminal() call for every token
//
//    - a <digits> nonterminal, which it outputs in addition to
//      the regular AddTerminal() call for each string of digits
//
//    - <N_digits> nonterminals, where N is the length of the string of
//      digits. By default the maximum N that will be output is 20. This
//      may be changed at compile time by kMaxNDigitsLength. For instance,
//      "123" will produce a <3_digits> nonterminal, "1234567" will produce
//      a <7_digits> nonterminal.
//
// It does not output any whitespace.  Instead, whitespace gets absorbed into
// the token that follows them in the text.
// For example, if the text contains:
//
//      ...hello                       there        world...
//              |                      |            |
//              offset=16              39           52
//
// then the output will be:
//
//      "hello" [?, 16)
//      "there" [16, 44)      <-- note "16" NOT "39"
//      "world" [44, ?)       <-- note "44" NOT "52"
//
// This makes it appear to the Matcher as if the tokens are adjacent -- so
// whitespace is simply ignored.
//
// A minor optimization:  We don't bother to output <token> unless
// the grammar includes a nonterminal with that name. Similarly, we don't
// output <digits> unless the grammar uses them.

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_LEXER_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_LEXER_H_

#include "annotator/types.h"
#include "utils/grammar/matcher.h"
#include "utils/grammar/rules_generated.h"
#include "utils/grammar/types.h"
#include "utils/strings/stringpiece.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3::grammar {

class Lexer {
 public:
  explicit Lexer(const UniLib& unilib) : unilib_(unilib) {}

  void Process(const std::vector<Token>& tokens, Matcher* matcher) const;

 private:
  enum TokenType {
    TOKEN_TYPE_TERM,
    TOKEN_TYPE_WHITESPACE,
    TOKEN_TYPE_DIGITS,
    TOKEN_TYPE_PUNCTUATION
  };

  // Processes a single token: the token is split, classified and passed to the
  // matcher. Returns the end of the last part emitted.
  int ProcessToken(const StringPiece value, const int prev_token_end,
                   const CodepointSpan codepoint_span,
                   const RulesSet_::Nonterminals* nonterms,
                   Matcher* matcher) const;

  // Emits a token to the matcher.
  void Emit(const StringPiece value, const CodepointSpan codepoint_span,
            int match_offset, const TokenType type,
            const RulesSet_::Nonterminals* nonterms, Matcher* matcher) const;

  // Gets the type of a character.
  TokenType GetTokenType(const UnicodeText::const_iterator& it) const;

 private:
  const UniLib& unilib_;
};

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_LEXER_H_
