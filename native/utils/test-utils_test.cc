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

#include "utils/test-utils.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

TEST(TestUtilTest, Utf8) {
  std::vector<Token> tokens =
      TokenizeOnSpace("Where is Jörg Borg located? Maybe in Zürich ...");

  EXPECT_EQ(tokens.size(), 9);

  EXPECT_EQ(tokens[0].value, "Where");
  EXPECT_EQ(tokens[0].start, 0);
  EXPECT_EQ(tokens[0].end, 5);

  EXPECT_EQ(tokens[1].value, "is");
  EXPECT_EQ(tokens[1].start, 6);
  EXPECT_EQ(tokens[1].end, 8);

  EXPECT_EQ(tokens[2].value, "Jörg");
  EXPECT_EQ(tokens[2].start, 9);
  EXPECT_EQ(tokens[2].end, 13);

  EXPECT_EQ(tokens[3].value, "Borg");
  EXPECT_EQ(tokens[3].start, 14);
  EXPECT_EQ(tokens[3].end, 18);

  EXPECT_EQ(tokens[4].value, "located?");
  EXPECT_EQ(tokens[4].start, 19);
  EXPECT_EQ(tokens[4].end, 27);

  EXPECT_EQ(tokens[5].value, "Maybe");
  EXPECT_EQ(tokens[5].start, 28);
  EXPECT_EQ(tokens[5].end, 33);

  EXPECT_EQ(tokens[6].value, "in");
  EXPECT_EQ(tokens[6].start, 34);
  EXPECT_EQ(tokens[6].end, 36);

  EXPECT_EQ(tokens[7].value, "Zürich");
  EXPECT_EQ(tokens[7].start, 37);
  EXPECT_EQ(tokens[7].end, 43);

  EXPECT_EQ(tokens[8].value, "...");
  EXPECT_EQ(tokens[8].start, 44);
  EXPECT_EQ(tokens[8].end, 47);
}

}  // namespace
}  // namespace libtextclassifier3
