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

// Utils for creating action suggestions.

#ifndef LIBTEXTCLASSIFIER_ACTIONS_UTILS_H_
#define LIBTEXTCLASSIFIER_ACTIONS_UTILS_H_

#include <string>
#include <vector>

#include "actions/actions_model_generated.h"
#include "actions/types.h"
#include "annotator/types.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

// Creates an action suggestion from a template with fallback defaults.
ActionSuggestion SuggestionFromSpec(
    const ActionSuggestionSpec* action, const std::string& default_type = "",
    const std::string& default_response_text = "",
    const std::string& default_serialized_entity_data = "",
    const float default_score = 0.0f,
    const float default_priority_score = 0.0f);

// Creates text replies from capturing matches.
void SuggestTextRepliesFromCapturingMatch(
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    const UnicodeText& match_text, const std::string& smart_reply_action_type,
    std::vector<ActionSuggestion>* actions);

// Applies normalization to a capturing match.
UnicodeText NormalizeMatchText(
    const UniLib* unilib,
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    StringPiece match_text);

// Fills the fields in an annotation from a capturing match.
bool FillAnnotationFromCapturingMatch(
    const CodepointSpan& span,
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    const int message_index, StringPiece match_text,
    ActionSuggestionAnnotation* annotation);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_UTILS_H_
