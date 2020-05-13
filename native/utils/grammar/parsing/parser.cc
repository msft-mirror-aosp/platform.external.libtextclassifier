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

#include "utils/grammar/parsing/parser.h"

#include "utils/grammar/callback-delegate.h"
#include "utils/grammar/match.h"
#include "utils/grammar/matcher.h"

namespace libtextclassifier3::grammar {
namespace {

class ParserCallbackDelegate : public CallbackDelegate {
 public:
  // Handles a grammar rule match.
  void MatchFound(const Match* match, const CallbackId type, const int64 value,
                  Matcher* matcher) override;

  // Returns the deduplicated root rule derivations.
  std::vector<Derivation> GetDerivations() const;

 private:
  // List of potential derivations of root rules.
  std::vector<Derivation> root_derivations_;
};

void ParserCallbackDelegate::MatchFound(const Match* match,
                                        const CallbackId type,
                                        const int64 value, Matcher* matcher) {
  switch (static_cast<Parser::Callback>(type)) {
    case Parser::Callback::kRootRule: {
      root_derivations_.push_back(Derivation{match, /*rule_id=*/value});
      break;
    }
    default: {
      CallbackDelegate::MatchFound(match, type, value, matcher);
      break;
    }
  }
}

std::vector<Derivation> ParserCallbackDelegate::GetDerivations() const {
  std::vector<Derivation> result;
  for (const Derivation& derivation :
       DeduplicateDerivations(root_derivations_)) {
    // Check that assertions are fulfilled.
    if (VerifyAssertions(derivation.match)) {
      result.push_back(derivation);
    }
  }
  return result;
}

}  // namespace

Parser::Parser(const UniLib* unilib, const RulesSet* rules)
    : unilib_(*unilib),
      rules_(rules),
      lexer_(unilib, rules),
      rules_locales_(ParseRulesLocales(rules_)) {}

// Parses an input text and returns the root rule derivations.
std::vector<Derivation> Parser::Parse(const TextContext& input) {
  // Select locale matching rules.
  std::vector<const RulesSet_::Rules*> locale_rules =
      SelectLocaleMatchingShards(rules_, rules_locales_, input.locales);

  if (locale_rules.empty()) {
    // Nothing to do.
    return {};
  }

  ParserCallbackDelegate callback_delegate;
  Matcher matcher(&unilib_, rules_, locale_rules, &callback_delegate);

  // Run the lexer on the text input.
  lexer_.Process(input.text, input.tokens,
                 /*annotations=*/&input.annotations, &matcher);

  return callback_delegate.GetDerivations();
}

}  // namespace libtextclassifier3::grammar
