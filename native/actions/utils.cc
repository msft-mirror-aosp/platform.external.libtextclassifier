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

#include "actions/utils.h"

#include "utils/base/logging.h"
#include "utils/normalization.h"

namespace libtextclassifier3 {

ActionSuggestion SuggestionFromSpec(
    const ActionSuggestionSpec* action, const std::string& default_type,
    const std::string& default_response_text,
    const std::string& default_serialized_entity_data,
    const float default_score, const float default_priority_score) {
  ActionSuggestion suggestion;
  suggestion.score = action != nullptr ? action->score() : default_score;
  suggestion.priority_score =
      action != nullptr ? action->priority_score() : default_priority_score;
  suggestion.type = action != nullptr && action->type() != nullptr
                        ? action->type()->str()
                        : default_type;
  suggestion.response_text =
      action != nullptr && action->response_text() != nullptr
          ? action->response_text()->str()
          : default_response_text;
  suggestion.serialized_entity_data =
      action != nullptr && action->serialized_entity_data() != nullptr
          ? action->serialized_entity_data()->str()
          : default_serialized_entity_data;
  return suggestion;
}

void SuggestTextRepliesFromCapturingMatch(
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    const UnicodeText& match_text, const std::string& smart_reply_action_type,
    std::vector<ActionSuggestion>* actions) {
  if (group->text_reply() != nullptr) {
    actions->push_back(
        SuggestionFromSpec(group->text_reply(),
                           /*default_type=*/smart_reply_action_type,
                           /*default_response_text=*/
                           match_text.ToUTF8String()));
  }
}

UnicodeText NormalizeMatchText(
    const UniLib* unilib,
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    StringPiece match_text) {
  UnicodeText normalized_match_text =
      UTF8ToUnicodeText(match_text, /*do_copy=*/false);
  if (group->normalization_options() != nullptr) {
    normalized_match_text = NormalizeText(
        unilib, group->normalization_options(), normalized_match_text);
  }
  return normalized_match_text;
}

bool FillAnnotationFromCapturingMatch(
    const CodepointSpan& span,
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    const int message_index, StringPiece match_text,
    ActionSuggestionAnnotation* annotation) {
  if (group->annotation_name() == nullptr &&
      group->annotation_type() == nullptr) {
    return false;
  }
  annotation->span.span = span;
  annotation->span.message_index = message_index;
  annotation->span.text = match_text.ToString();
  if (group->annotation_name() != nullptr) {
    annotation->name = group->annotation_name()->str();
  }
  if (group->annotation_type() != nullptr) {
    annotation->entity.collection = group->annotation_type()->str();
  }
  return true;
}

}  // namespace libtextclassifier3
