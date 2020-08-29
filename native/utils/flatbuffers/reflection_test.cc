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

#include "utils/flatbuffers/reflection.h"

#include "utils/flatbuffers/flatbuffers_generated.h"
#include "utils/flatbuffers/test-utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "flatbuffers/reflection_generated.h"

namespace libtextclassifier3 {
namespace {

TEST(ReflectionTest, ResolvesFieldOffsets) {
  std::string metadata_buffer = LoadTestMetadata();
  const reflection::Schema* schema =
      flatbuffers::GetRoot<reflection::Schema>(metadata_buffer.data());
  FlatbufferFieldPathT path;
  path.field.emplace_back(new FlatbufferFieldT);
  path.field.back()->field_name = "flight_number";
  path.field.emplace_back(new FlatbufferFieldT);
  path.field.back()->field_name = "carrier_code";

  EXPECT_TRUE(SwapFieldNamesForOffsetsInPath(schema, &path));

  EXPECT_THAT(path.field[0]->field_name, testing::IsEmpty());
  EXPECT_EQ(14, path.field[0]->field_offset);
  EXPECT_THAT(path.field[1]->field_name, testing::IsEmpty());
  EXPECT_EQ(4, path.field[1]->field_offset);
}

}  // namespace
}  // namespace libtextclassifier3
