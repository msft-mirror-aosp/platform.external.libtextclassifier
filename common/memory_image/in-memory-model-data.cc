/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "common/memory_image/in-memory-model-data.h"

#include "common/file-utils.h"
#include "util/base/logging.h"
#include "util/strings/stringpiece.h"

namespace libtextclassifier {
namespace nlp_core {

const char InMemoryModelData::kTaskSpecDataStoreEntryName[] = "TASK-SPEC-#@";
const char InMemoryModelData::kFilePatternPrefix[] = "in-mem-model::";

bool InMemoryModelData::GetTaskSpec(TaskSpec *task_spec) const {
  StringPiece blob = data_store_.GetData(kTaskSpecDataStoreEntryName);
  if (blob.data() == nullptr) {
    TC_LOG(ERROR) << "Can't find data blob for TaskSpec, i.e., entry "
                  << kTaskSpecDataStoreEntryName;
    return false;
  }
  bool parse_status = file_utils::ParseProtoFromMemory(blob, task_spec);
  if (!parse_status) {
    TC_LOG(ERROR) << "Error parsing TaskSpec";
    return false;
  }
  return true;
}

}  // namespace nlp_core
}  // namespace libtextclassifier
