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

#include "annotator/grammar/grammar-annotator.h"

#include "annotator/types.h"
#include "utils/base/logging.h"
#include "utils/grammar/callback-delegate.h"
#include "utils/grammar/match.h"
#include "utils/grammar/matcher.h"
#include "utils/grammar/rules-utils.h"
#include "utils/grammar/types.h"

namespace libtextclassifier3 {
namespace {

class GrammarAnnotatorCallbackDelegate : public grammar::CallbackDelegate {
 public:
  explicit GrammarAnnotatorCallbackDelegate(const GrammarModel* model)
      : model_(model) {}

  // Handles a grammar rule match in the annotator grammar.
  void MatchFound(const grammar::Match* match, grammar::CallbackId type,
                  int64 value, grammar::Matcher* matcher) override {
    switch (static_cast<GrammarAnnotator::Callback>(type)) {
      case GrammarAnnotator::Callback::kRuleMatch: {
        HandleRuleMatch(match, /*rule_id=*/value);
        return;
      }
      case GrammarAnnotator::Callback::kCapturingMatch: {
        HandleCapturingMatch(match, /*match_id=*/value, matcher);
        return;
      }
      case GrammarAnnotator::Callback::kAssertionMatch: {
        HandleAssertion(match, /*negative=*/(value != 0), matcher);
        return;
      }
      default:
        TC3_LOG(ERROR) << "Unhandled match type: " << type;
    }
  }

  // Deduplicate and populate annotations from grammar matches.
  bool GetAnnotations(std::vector<AnnotatedSpan>* annotations) const {
    for (const grammar::RuleMatch& candidate :
         grammar::DeduplicateMatches(candidates_)) {
      // Check that assertions are fulfilled.
      if (!grammar::VerifyAssertions(candidate.match)) {
        continue;
      }
      if (!InstantiateAnnotatedSpanFromMatch(candidate, annotations)) {
        return false;
      }
    }
    return true;
  }

 private:
  // Handles annotation/selection/classification rule matches.
  void HandleRuleMatch(const grammar::Match* match, const int64 rule_id) {
    candidates_.push_back(grammar::RuleMatch{match, rule_id});
  }

  // Computes the selection boundaries from a grammar match.
  CodepointSpan MatchSelectionBoundaries(
      const grammar::Match* match,
      const GrammarModel_::RuleClassificationResult* classification) const {
    if (classification->capturing_group() == nullptr) {
      // Use full match as selection span.
      return match->codepoint_span;
    }

    // Set information from capturing matches.
    CodepointSpan span{kInvalidIndex, kInvalidIndex};
    const std::unordered_map<uint16, const grammar::CapturingMatch*>
        capturing_matches = GatherCapturingMatches(match);

    // Compute span boundaries.
    for (int i = 0; i < classification->capturing_group()->size(); i++) {
      auto it = capturing_matches.find(i);
      if (it == capturing_matches.end()) {
        // Capturing group is not active, skip.
        continue;
      }
      const CapturingGroup* group = classification->capturing_group()->Get(i);
      if (group->extend_selection()) {
        if (span.first == kInvalidIndex) {
          span = it->second->codepoint_span;
        } else {
          span.first = std::min(span.first, it->second->codepoint_span.first);
          span.second =
              std::max(span.second, it->second->codepoint_span.second);
        }
      }
    }
    return span;
  }

  // Instantiates an annotated span from a rule match and appends it to the
  // result.
  bool InstantiateAnnotatedSpanFromMatch(
      const grammar::RuleMatch& candidate,
      std::vector<AnnotatedSpan>* result) const {
    if (candidate.rule_id < 0 ||
        candidate.rule_id >= model_->rule_classification_result()->size()) {
      TC3_LOG(INFO) << "Invalid rule id.";
      return false;
    }
    const GrammarModel_::RuleClassificationResult* classification =
        model_->rule_classification_result()->Get(candidate.rule_id);
    result->emplace_back();
    result->back().span =
        MatchSelectionBoundaries(candidate.match, classification);
    result->back().classification = {{
        classification->collection_name()->str(),
        classification->target_classification_score(),
        classification->priority_score(),
    }};
    return true;
  }

  const GrammarModel* model_;

  // All annotation/selection/classification rule match candidates.
  // Grammar rule matches are recorded, deduplicated and then instantiated.
  std::vector<grammar::RuleMatch> candidates_;
};

}  // namespace

GrammarAnnotator::GrammarAnnotator(const UniLib* unilib,
                                   const GrammarModel* model)
    : unilib_(unilib),
      model_(model),
      lexer_(*unilib),
      rules_locales_(grammar::ParseRulesLocales(model->rules())) {}

bool GrammarAnnotator::Annotate(const std::vector<Locale>& locales,
                                const std::vector<Token>& tokens,
                                std::vector<AnnotatedSpan>* result) const {
  if (model_ == nullptr || model_->rules() == nullptr) {
    // Nothing to do.
    return true;
  }

  GrammarAnnotatorCallbackDelegate callback_handler(model_);

  // Select locale matching rules.
  std::vector<grammar::RulesCallbackDelegate> locale_rules;
  for (int i = 0; i < rules_locales_.size(); i++) {
    if (rules_locales_[i].empty() ||
        Locale::IsAnyLocaleSupported(locales,
                                     /*supported_locales=*/rules_locales_[i],
                                     /*default_value=*/false)) {
      locale_rules.push_back(
          {model_->rules()->rules()->Get(i), &callback_handler});
    }
  }

  // Run the grammar.
  grammar::Matcher matcher(*unilib_, model_->rules(), locale_rules);
  lexer_.Process(tokens, &matcher);

  // Populate results.
  return callback_handler.GetAnnotations(result);
}

}  // namespace libtextclassifier3
