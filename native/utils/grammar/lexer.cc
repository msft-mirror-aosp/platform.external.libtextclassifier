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

#include "utils/grammar/lexer.h"

namespace libtextclassifier3::grammar {
namespace {

inline bool CheckMemoryUsage(const Matcher* matcher) {
  // The maximum memory usage for matching.
  constexpr int kMaxMemoryUsage = 1 << 20;
  return matcher->ArenaSize() <= kMaxMemoryUsage;
}

void CheckedEmit(const Nonterm nonterm, const CodepointSpan codepoint_span,
                 const int match_offset, int16 type, Matcher* matcher) {
  if (CheckMemoryUsage(matcher)) {
    matcher->AddMatch(matcher->AllocateAndInitMatch<Match>(
        nonterm, codepoint_span, match_offset, type));
  }
}

}  // namespace

void Lexer::Emit(const StringPiece value, const CodepointSpan codepoint_span,
                 const int match_offset, const TokenType type,
                 const RulesSet_::Nonterminals* nonterms,
                 Matcher* matcher) const {
  // Emit the token as terminal.
  if (CheckMemoryUsage(matcher)) {
    matcher->AddTerminal(codepoint_span, match_offset, value);
  }

  // Emit <token> if used by rules.
  if (nonterms->token_nt() != kUnassignedNonterm) {
    CheckedEmit(nonterms->token_nt(), codepoint_span, match_offset,
                Match::kTokenType, matcher);
  }

  // Emit token type specific non-terminals.
  if (type == TOKEN_TYPE_DIGITS) {
    // Emit <digits> if used by the rules.
    if (nonterms->digits_nt() != kUnassignedNonterm) {
      CheckedEmit(nonterms->digits_nt(), codepoint_span, match_offset,
                  Match::kDigitsType, matcher);
    }

    // Emit <n_digits> if used by the rules.
    if (nonterms->n_digits_nt() != nullptr) {
      const int num_digits = codepoint_span.second - codepoint_span.first;
      if (num_digits <= nonterms->n_digits_nt()->size()) {
        if (const Nonterm n_digits =
                nonterms->n_digits_nt()->Get(num_digits - 1)) {
          CheckedEmit(n_digits, codepoint_span, match_offset,
                      Match::kDigitsType, matcher);
        }
      }
    }
  }
}

Lexer::TokenType Lexer::GetTokenType(
    const UnicodeText::const_iterator& it) const {
  if (unilib_.IsPunctuation(*it)) {
    return TOKEN_TYPE_PUNCTUATION;
  } else if (unilib_.IsDigit(*it)) {
    return TOKEN_TYPE_DIGITS;
  } else if (unilib_.IsWhitespace(*it)) {
    return TOKEN_TYPE_WHITESPACE;
  } else {
    return TOKEN_TYPE_TERM;
  }
}

int Lexer::ProcessToken(const StringPiece value, const int prev_token_end,
                        const CodepointSpan codepoint_span,
                        const RulesSet_::Nonterminals* nonterms,
                        Matcher* matcher) const {
  UnicodeText token_unicode = UTF8ToUnicodeText(value.data(), value.size(),
                                                /*do_copy=*/false);

  if (unilib_.IsWhitespace(*token_unicode.begin())) {
    // Ignore whitespace tokens.
    return prev_token_end;
  }

  // Possibly split token.
  int last_end = prev_token_end;
  auto token_end = token_unicode.end();
  auto it = token_unicode.begin();
  TokenType type = GetTokenType(it);
  CodepointIndex sub_token_start = codepoint_span.first;
  while (it != token_end) {
    auto next = std::next(it);
    int num_codepoints = 1;
    TokenType next_type;
    while (next != token_end) {
      next_type = GetTokenType(next);
      if (type == TOKEN_TYPE_PUNCTUATION || next_type != type) {
        break;
      }
      ++next;
      ++num_codepoints;
    }

    // Emit token.
    StringPiece sub_token =
        StringPiece(it.utf8_data(), next.utf8_data() - it.utf8_data());
    if (type != TOKEN_TYPE_WHITESPACE) {
      Emit(sub_token,
           CodepointSpan{sub_token_start, sub_token_start + num_codepoints},
           /*match_offset=*/last_end, type, nonterms, matcher);
      last_end = sub_token_start + num_codepoints;
    }
    it = next;
    type = next_type;
    sub_token_start = last_end;
  }
  return last_end;
}

void Lexer::Process(const std::vector<Token>& tokens, Matcher* matcher) const {
  if (tokens.empty()) {
    return;
  }

  const RulesSet_::Nonterminals* nonterminals = matcher->nonterminals();

  // Initialize processing of new text.
  int prev_token_end = 0;
  matcher->Reset();

  // Emit start symbol if used by the grammar.
  if (nonterminals->start_nt() != kUnassignedNonterm) {
    matcher->AddMatch(matcher->AllocateAndInitMatch<Match>(
        nonterminals->start_nt(), CodepointSpan{0, 0},
        /*match_offset=*/0));
  }

  for (const Token& token : tokens) {
    prev_token_end = ProcessToken(token.value,
                                  /*prev_token_end=*/prev_token_end,
                                  CodepointSpan{token.start, token.end},
                                  nonterminals, matcher);
  }

  // Emit end symbol if used by the grammar.
  if (nonterminals->end_nt() != kUnassignedNonterm) {
    matcher->AddMatch(matcher->AllocateAndInitMatch<Match>(
        nonterminals->end_nt(), CodepointSpan{prev_token_end, prev_token_end},
        /*match_offset=*/prev_token_end));
  }
}

}  // namespace libtextclassifier3::grammar
