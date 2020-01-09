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

#include "utils/strings/utf8.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using testing::Eq;

TEST(Utf8Test, GetNumBytesForUTF8Char) {
  EXPECT_THAT(GetNumBytesForUTF8Char("\x00"), Eq(0));
  EXPECT_THAT(GetNumBytesForUTF8Char("h"), Eq(1));
  EXPECT_THAT(GetNumBytesForUTF8Char("😋"), Eq(4));
  EXPECT_THAT(GetNumBytesForUTF8Char("㍿"), Eq(3));
}

TEST(Utf8Test, IsValidUTF8) {
  EXPECT_TRUE(IsValidUTF8("1234😋hello", 13));
  EXPECT_TRUE(IsValidUTF8("\u304A\u00B0\u106B", 8));
  EXPECT_TRUE(IsValidUTF8("this is a test😋😋😋", 26));
  EXPECT_TRUE(IsValidUTF8("\xf0\x9f\x98\x8b", 4));
  // Too short (string is too short).
  EXPECT_FALSE(IsValidUTF8("\xf0\x9f", 2));
  // Too long (too many trailing bytes).
  EXPECT_FALSE(IsValidUTF8("\xf0\x9f\x98\x8b\x8b", 5));
  // Too short (too few trailing bytes).
  EXPECT_FALSE(IsValidUTF8("\xf0\x9f\x98\x61\x61", 5));
}

TEST(Utf8Test, ValidUTF8CharLength) {
  EXPECT_THAT(ValidUTF8CharLength("1234😋hello", 13), Eq(1));
  EXPECT_THAT(ValidUTF8CharLength("\u304A\u00B0\u106B", 8), Eq(3));
  EXPECT_THAT(ValidUTF8CharLength("this is a test😋😋😋", 26), Eq(1));
  EXPECT_THAT(ValidUTF8CharLength("\xf0\x9f\x98\x8b", 4), Eq(4));
  // Too short (string is too short).
  EXPECT_THAT(ValidUTF8CharLength("\xf0\x9f", 2), Eq(-1));
  // Too long (too many trailing bytes). First character is valid.
  EXPECT_THAT(ValidUTF8CharLength("\xf0\x9f\x98\x8b\x8b", 5), Eq(4));
  // Too short (too few trailing bytes).
  EXPECT_THAT(ValidUTF8CharLength("\xf0\x9f\x98\x61\x61", 5), Eq(-1));
}

TEST(Utf8Test, CorrectlyTruncatesStrings) {
  EXPECT_THAT(SafeTruncateLength("FooBar", 3), Eq(3));
  EXPECT_THAT(SafeTruncateLength("früh", 3), Eq(2));
  EXPECT_THAT(SafeTruncateLength("مَمِمّمَّمِّ", 5), Eq(4));
}

TEST(Utf8Test, CorrectlyConvertsFromUtf8) {
  EXPECT_THAT(ValidCharToRune("a"), Eq(97));
  EXPECT_THAT(ValidCharToRune("\0"), Eq(0));
  EXPECT_THAT(ValidCharToRune("\u304A"), Eq(0x304a));
  EXPECT_THAT(ValidCharToRune("\xe3\x81\x8a"), Eq(0x304a));
}

TEST(Utf8Test, CorrectlyConvertsToUtf8) {
  char utf8_encoding[4];
  EXPECT_THAT(ValidRuneToChar(97, utf8_encoding), Eq(1));
  EXPECT_THAT(ValidRuneToChar(0, utf8_encoding), Eq(1));
  EXPECT_THAT(ValidRuneToChar(0x304a, utf8_encoding), Eq(3));
}

}  // namespace
}  // namespace libtextclassifier3
