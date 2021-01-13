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

// This is a shared test between icu and javaicu calendar implementations.
// It is meant to be #include'd.

#ifndef LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_TEST_INCLUDE_H_
#define LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_TEST_INCLUDE_H_
#include "utils/jvm-test-utils.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace test_internal {

class CalendarTest : public ::testing::Test {
 protected:
  CalendarTest()
      : calendarlib_(libtextclassifier3::CreateCalendarLibForTesting()) {}
  std::unique_ptr<CalendarLib> calendarlib_;
};

}  // namespace test_internal
}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_TEST_INCLUDE_H_
