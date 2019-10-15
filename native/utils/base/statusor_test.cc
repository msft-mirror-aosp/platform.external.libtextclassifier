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

#include "utils/base/statusor.h"

#include "utils/base/logging.h"
#include "utils/base/status.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

TEST(StatusOrTest, DoesntDieWhenOK) {
  StatusOr<std::string> status_or_string = std::string("Hello World");
  EXPECT_TRUE(status_or_string.ok());
  EXPECT_EQ(status_or_string.ValueOrDie(), "Hello World");
}

TEST(StatusOrTest, DiesWhenNotOK) {
  StatusOr<std::string> status_or_string = {Status::UNKNOWN};
  EXPECT_FALSE(status_or_string.ok());
  EXPECT_DEATH(status_or_string.ValueOrDie(),
               "Attempting to fetch value of non-OK StatusOr: 2");
}

}  // namespace
}  // namespace libtextclassifier3
