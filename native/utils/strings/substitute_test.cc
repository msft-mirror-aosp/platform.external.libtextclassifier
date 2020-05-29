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

#include "utils/strings/substitute.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {
namespace {

TEST(SubstituteTest, Substitute) {
  EXPECT_EQ("Hello, world!",
            strings::Substitute("$0, $1!", {"Hello", "world"}));

  // Out of order.
  EXPECT_EQ("world, Hello!",
            strings::Substitute("$1, $0!", {"Hello", "world"}));
  EXPECT_EQ("b, a, c, b",
            strings::Substitute("$1, $0, $2, $1", {"a", "b", "c"}));

  // Literal $
  EXPECT_EQ("$", strings::Substitute("$$", {}));
  EXPECT_EQ("$1", strings::Substitute("$$1", {}));

  const char* null_cstring = nullptr;
  EXPECT_EQ("Text: ''", strings::Substitute("Text: '$0'", {null_cstring}));
}

}  // namespace
}  // namespace libtextclassifier3
