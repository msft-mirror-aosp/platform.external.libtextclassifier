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

#include "utils/base/status.h"

#include "utils/base/logging.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

TEST(StatusTest, PrintsAbortedStatus) {
  logging::LoggingStringStream stream;
  stream << Status::UNKNOWN;
  EXPECT_EQ(Status::UNKNOWN.error_code(), 2);
  EXPECT_EQ(Status::UNKNOWN.CanonicalCode(), StatusCode::UNKNOWN);
  EXPECT_EQ(Status::UNKNOWN.error_message(), "");
  EXPECT_EQ(stream.message, "2");
}

TEST(StatusTest, PrintsOKStatus) {
  logging::LoggingStringStream stream;
  stream << Status::OK;
  EXPECT_EQ(Status::OK.error_code(), 0);
  EXPECT_EQ(Status::OK.CanonicalCode(), StatusCode::OK);
  EXPECT_EQ(Status::OK.error_message(), "");
  EXPECT_EQ(stream.message, "0");
}

TEST(StatusTest, UnknownStatusHasRightAttributes) {
  EXPECT_EQ(Status::UNKNOWN.error_code(), 2);
  EXPECT_EQ(Status::UNKNOWN.CanonicalCode(), StatusCode::UNKNOWN);
  EXPECT_EQ(Status::UNKNOWN.error_message(), "");
}

TEST(StatusTest, OkStatusHasRightAttributes) {
  EXPECT_EQ(Status::OK.error_code(), 0);
  EXPECT_EQ(Status::OK.CanonicalCode(), StatusCode::OK);
  EXPECT_EQ(Status::OK.error_message(), "");
}

TEST(StatusTest, CustomStatusHasRightAttributes) {
  Status status(StatusCode::INVALID_ARGUMENT, "You can't put this here!");
  EXPECT_EQ(status.error_code(), 3);
  EXPECT_EQ(status.CanonicalCode(), StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status.error_message(), "You can't put this here!");
}

TEST(StatusTest, AssignmentPreservesMembers) {
  Status status(StatusCode::INVALID_ARGUMENT, "You can't put this here!");

  Status status2 = status;

  EXPECT_EQ(status2.error_code(), 3);
  EXPECT_EQ(status2.CanonicalCode(), StatusCode::INVALID_ARGUMENT);
  EXPECT_EQ(status2.error_message(), "You can't put this here!");
}

}  // namespace
}  // namespace libtextclassifier3
