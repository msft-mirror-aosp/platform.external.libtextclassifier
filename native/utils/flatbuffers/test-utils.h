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

// Common test utils.

#ifndef LIBTEXTCLASSIFIER_UTILS_FLATBUFFERS_TEST_UTILS_H_
#define LIBTEXTCLASSIFIER_UTILS_FLATBUFFERS_TEST_UTILS_H_

#include <fstream>
#include <string>

#include "utils/flatbuffers/flatbuffers_generated.h"
#include "utils/strings/split.h"
#include "utils/strings/stringpiece.h"
#include "utils/test-data-test-utils.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {

inline std::string LoadTestMetadata() {
  std::ifstream test_config_stream(
      GetTestDataPath("utils/flatbuffers/flatbuffers_test_extended.bfbs"));
  return std::string((std::istreambuf_iterator<char>(test_config_stream)),
                     (std::istreambuf_iterator<char>()));
}

// Creates a flatbuffer field path from a dot separated field path string.
inline std::unique_ptr<FlatbufferFieldPathT> CreateFieldPath(
    const StringPiece path) {
  std::unique_ptr<FlatbufferFieldPathT> field_path(new FlatbufferFieldPathT);
  for (const StringPiece field : strings::Split(path, '.')) {
    field_path->field.emplace_back(new FlatbufferFieldT);
    field_path->field.back()->field_name = field.ToString();
  }
  return field_path;
}

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_FLATBUFFERS_TEST_UTILS_H_
