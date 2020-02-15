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

#include "actions/flatbuffer-utils.h"

#include <memory>

#include "utils/base/logging.h"
#include "utils/flatbuffers.h"
#include "flatbuffers/reflection.h"

namespace libtextclassifier3 {

bool SwapFieldNamesForOffsetsInPathInActionsModel(ActionsModelT* model) {
  if (model->actions_entity_data_schema.empty()) {
    // Nothing to do.
    return true;
  }

  const reflection::Schema* schema =
      LoadAndVerifyFlatbuffer<reflection::Schema>(
          model->actions_entity_data_schema.data(),
          model->actions_entity_data_schema.size());

  // Resolve offsets in regex rules.
  if (model->rules != nullptr) {
    for (std::unique_ptr<RulesModel_::RegexRuleT>& rule :
         model->rules->regex_rule) {
      for (std::unique_ptr<RulesModel_::RuleActionSpecT>& rule_action :
           rule->actions) {
        for (std::unique_ptr<RulesModel_::RuleActionSpec_::RuleCapturingGroupT>&
                 capturing_group : rule_action->capturing_group) {
          if (capturing_group->entity_field == nullptr) {
            continue;
          }
          if (!SwapFieldNamesForOffsetsInPath(
                  schema, capturing_group->entity_field.get())) {
            return false;
          }
        }
      }
    }
  }

  // Resolve offsets in annotation action mapping.
  if (model->annotation_actions_spec != nullptr) {
    for (std::unique_ptr<AnnotationActionsSpec_::AnnotationMappingT>& mapping :
         model->annotation_actions_spec->annotation_mapping) {
      if (mapping->entity_field == nullptr) {
        continue;
      }
      if (!SwapFieldNamesForOffsetsInPath(schema,
                                          mapping->entity_field.get())) {
        return false;
      }
    }
  }

  return true;
}

std::string SwapFieldNamesForOffsetsInPathInSerializedActionsModel(
    const std::string& model) {
  std::unique_ptr<ActionsModelT> unpacked_model =
      UnPackActionsModel(model.c_str());
  TC3_CHECK(unpacked_model != nullptr);
  TC3_CHECK(SwapFieldNamesForOffsetsInPathInActionsModel(unpacked_model.get()));
  flatbuffers::FlatBufferBuilder builder;
  FinishActionsModelBuffer(builder,
                           ActionsModel::Pack(builder, unpacked_model.get()));
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

}  // namespace libtextclassifier3
