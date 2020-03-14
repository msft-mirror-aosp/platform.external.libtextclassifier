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

#include <unordered_map>

namespace libtextclassifier3::grammar {
namespace {

inline bool CheckMemoryUsage(const Matcher* matcher) {
  // The maximum memory usage for matching.
  constexpr int kMaxMemoryUsage = 1 << 20;
  return matcher->ArenaSize() <= kMaxMemoryUsage;
}

Match* CheckedAddMatch(const Nonterm nonterm,
                       const CodepointSpan codepoint_span,
                       const int match_offset, const int16 type,
                       Matcher* matcher) {
  if (nonterm == kUnassignedNonterm || !CheckMemoryUsage(matcher)) {
    return nullptr;
  }
  return matcher->AllocateAndInitMatch<Match>(nonterm, codepoint_span,
                                              match_offset, type);
}

void CheckedEmit(const Nonterm nonterm, const CodepointSpan codepoint_span,
                 const int match_offset, int16 type, Matcher* matcher) {
  if (nonterm != kUnassignedNonterm && CheckMemoryUsage(matcher)) {
    matcher->AddMatch(matcher->AllocateAndInitMatch<Match>(
        nonterm, codepoint_span, match_offset, type));
  }
}

}  // namespace

void Lexer::Emit(const Symbol& symbol, const RulesSet_::Nonterminals* nonterms,
                 Matcher* matcher) const {
  switch (symbol.type) {
    case Symbol::Type::TYPE_MATCH: {
      // Just emit the match.
      matcher->AddMatch(symbol.match);
      return;
    }
    case Symbol::Type::TYPE_DIGITS: {
      // Emit <digits> if used by the rules.
      CheckedEmit(nonterms->digits_nt(), symbol.codepoint_span,
                  symbol.match_offset, Match::kDigitsType, matcher);

      // Emit <n_digits> if used by the rules.
      if (nonterms->n_digits_nt() != nullptr) {
        const int num_digits =
            symbol.codepoint_span.second - symbol.codepoint_span.first;
        if (num_digits <= nonterms->n_digits_nt()->size()) {
          CheckedEmit(nonterms->n_digits_nt()->Get(num_digits - 1),
                      symbol.codepoint_span, symbol.match_offset,
                      Match::kDigitsType, matcher);
        }
      }
      break;
    }
    case Symbol::Type::TYPE_TERM: {
      // Emit <uppercase_token> if used by the rules.
      if (nonterms->uppercase_token_nt() != 0 &&
          unilib_.IsUpperText(
              UTF8ToUnicodeText(symbol.lexeme, /*do_copy=*/false))) {
        CheckedEmit(nonterms->uppercase_token_nt(), symbol.codepoint_span,
                    symbol.match_offset, Match::kTokenType, matcher);
      }
      break;
    }
    default:
      break;
  }

  // Emit the token as terminal.
  if (CheckMemoryUsage(matcher)) {
    matcher->AddTerminal(symbol.codepoint_span, symbol.match_offset,
                         symbol.lexeme);
  }

  // Emit <token> if used by rules.
  CheckedEmit(nonterms->token_nt(), symbol.codepoint_span, symbol.match_offset,
              Match::kTokenType, matcher);
}

Lexer::Symbol::Type Lexer::GetSymbolType(
    const UnicodeText::const_iterator& it) const {
  if (unilib_.IsPunctuation(*it)) {
    return Symbol::Type::TYPE_PUNCTUATION;
  } else if (unilib_.IsDigit(*it)) {
    return Symbol::Type::TYPE_DIGITS;
  } else {
    return Symbol::Type::TYPE_TERM;
  }
}

void Lexer::ProcessToken(const StringPiece value, const int prev_token_end,
                         const CodepointSpan codepoint_span,
                         std::vector<Lexer::Symbol>* symbols) const {
  // Possibly split token.
  UnicodeText token_unicode = UTF8ToUnicodeText(value.data(), value.size(),
                                                /*do_copy=*/false);
  int last_end = prev_token_end;
  auto token_end = token_unicode.end();
  auto it = token_unicode.begin();
  Symbol::Type type = GetSymbolType(it);
  CodepointIndex sub_token_start = codepoint_span.first;
  while (it != token_end) {
    auto next = std::next(it);
    int num_codepoints = 1;
    Symbol::Type next_type;
    while (next != token_end) {
      next_type = GetSymbolType(next);
      if (type == Symbol::Type::TYPE_PUNCTUATION || next_type != type) {
        break;
      }
      ++next;
      ++num_codepoints;
    }
    symbols->push_back(Symbol{
        type, CodepointSpan{sub_token_start, sub_token_start + num_codepoints},
        /*match_offset=*/last_end,
        /*lexeme=*/
        StringPiece(it.utf8_data(), next.utf8_data() - it.utf8_data())});
    last_end = sub_token_start + num_codepoints;
    it = next;
    type = next_type;
    sub_token_start = last_end;
  }
}

void Lexer::Process(const std::vector<Token>& tokens,
                    const std::vector<Match*>& matches,
                    Matcher* matcher) const {
  return Process(tokens.begin(), tokens.end(), matches, matcher);
}

void Lexer::Process(const std::vector<Token>::const_iterator& begin,
                    const std::vector<Token>::const_iterator& end,
                    const std::vector<Match*>& matches,
                    Matcher* matcher) const {
  if (begin == end) {
    return;
  }

  const RulesSet_::Nonterminals* nonterminals = matcher->nonterminals();

  // Initialize processing of new text.
  CodepointIndex prev_token_end = 0;
  std::vector<Symbol> symbols;
  matcher->Reset();

  // The matcher expects the terminals and non-terminals it received to be in
  // non-decreasing end-position order. The sorting above makes sure the
  // pre-defined matches adhere to that order.
  // Ideally, we would just have to emit a predefined match whenever we see that
  // the next token we feed would be ending later.
  // But as we implicitly ignore whitespace, we have to merge preceding
  // whitespace to the match start so that tokens and non-terminals fed appear
  // as next to each other without whitespace.
  // We keep track of real token starts and precending whitespace in
  // `token_match_start`, so that we can extend a predefined match's start to
  // include the preceding whitespace.
  std::unordered_map<CodepointIndex, CodepointIndex> token_match_start;

  // Add start symbols.
  if (Match* match =
          CheckedAddMatch(nonterminals->start_nt(), CodepointSpan{0, 0},
                          /*match_offset=*/0, Match::kBreakType, matcher)) {
    symbols.push_back(Symbol(match));
  }
  if (Match* match =
          CheckedAddMatch(nonterminals->wordbreak_nt(), CodepointSpan{0, 0},
                          /*match_offset=*/0, Match::kBreakType, matcher)) {
    symbols.push_back(Symbol(match));
  }

  for (auto token_it = begin; token_it != end; token_it++) {
    const Token& token = *token_it;

    // Record match starts for token boundaries, so that we can snap pre-defined
    // matches to it.
    if (prev_token_end != token.start) {
      token_match_start[token.start] = prev_token_end;
    }

    ProcessToken(token.value,
                 /*prev_token_end=*/prev_token_end,
                 CodepointSpan{token.start, token.end}, &symbols);
    prev_token_end = token.end;

    // Add word break symbol if used by the grammar.
    if (Match* match = CheckedAddMatch(
            nonterminals->wordbreak_nt(), CodepointSpan{token.end, token.end},
            /*match_offset=*/token.end, Match::kBreakType, matcher)) {
      symbols.push_back(Symbol(match));
    }
  }

  // Add end symbol if used by the grammar.
  if (Match* match = CheckedAddMatch(
          nonterminals->end_nt(), CodepointSpan{prev_token_end, prev_token_end},
          /*match_offset=*/prev_token_end, Match::kBreakType, matcher)) {
    symbols.push_back(Symbol(match));
  }

  // Add predefined matches.
  for (Match* match : matches) {
    // Decrease match offset to include preceding whitespace.
    auto token_match_start_it = token_match_start.find(match->match_offset);
    if (token_match_start_it != token_match_start.end()) {
      match->match_offset = token_match_start_it->second;
    }
    symbols.push_back(Symbol(match));
  }

  std::sort(symbols.begin(), symbols.end(),
            [](const Symbol& a, const Symbol& b) {
              // Sort by increasing (end, start) position to guarantee the
              // matcher requirement that the tokens are fed in non-decreasing
              // end position order.
              return std::tie(a.codepoint_span.second, a.codepoint_span.first) <
                     std::tie(b.codepoint_span.second, b.codepoint_span.first);
            });

  // Emit symbols to matcher.
  for (const Symbol& symbol : symbols) {
    Emit(symbol, nonterminals, matcher);
  }
}

}  // namespace libtextclassifier3::grammar
