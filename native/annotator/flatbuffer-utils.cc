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

#include "annotator/flatbuffer-utils.h"

#include <memory>

#include "utils/base/logging.h"
#include "utils/flatbuffers.h"
#include "flatbuffers/reflection.h"

namespace libtextclassifier3 {

bool SwapFieldNamesForOffsetsInPath(ModelT* model) {
  if (model->regex_model == nullptr || model->entity_data_schema.empty()) {
    // Nothing to do.
    return true;
  }
  const reflection::Schema* schema =
      LoadAndVerifyFlatbuffer<reflection::Schema>(
          model->entity_data_schema.data(), model->entity_data_schema.size());

  for (std::unique_ptr<RegexModel_::PatternT>& pattern :
       model->regex_model->patterns) {
    for (std::unique_ptr<CapturingGroupT>& group : pattern->capturing_group) {
      if (group->entity_field_path == nullptr) {
        continue;
      }

      if (!SwapFieldNamesForOffsetsInPath(schema,
                                          group->entity_field_path.get())) {
        return false;
      }
    }
  }

  return true;
}

std::string SwapFieldNamesForOffsetsInPathInSerializedModel(
    const std::string& model) {
  std::unique_ptr<ModelT> unpacked_model = UnPackModel(model.c_str());
  TC3_CHECK(unpacked_model != nullptr);
  TC3_CHECK(SwapFieldNamesForOffsetsInPath(unpacked_model.get()));
  flatbuffers::FlatBufferBuilder builder;
  FinishModelBuffer(builder, Model::Pack(builder, unpacked_model.get()));
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

}  // namespace libtextclassifier3
