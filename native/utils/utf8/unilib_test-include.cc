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

#include "utils/utf8/unilib_test-include.h"

#include "utils/base/logging.h"
#include "gmock/gmock.h"

namespace libtextclassifier3 {
namespace test_internal {

using ::testing::ElementsAre;

TEST_F(UniLibTest, CharacterClassesAscii) {
  EXPECT_TRUE(unilib_.IsOpeningBracket('('));
  EXPECT_TRUE(unilib_.IsClosingBracket(')'));
  EXPECT_FALSE(unilib_.IsWhitespace(')'));
  EXPECT_TRUE(unilib_.IsWhitespace(' '));
  EXPECT_FALSE(unilib_.IsDigit(')'));
  EXPECT_TRUE(unilib_.IsDigit('0'));
  EXPECT_TRUE(unilib_.IsDigit('9'));
  EXPECT_FALSE(unilib_.IsUpper(')'));
  EXPECT_TRUE(unilib_.IsUpper('A'));
  EXPECT_TRUE(unilib_.IsUpper('Z'));
  EXPECT_FALSE(unilib_.IsLower(')'));
  EXPECT_TRUE(unilib_.IsLower('a'));
  EXPECT_TRUE(unilib_.IsLower('z'));
  EXPECT_TRUE(unilib_.IsPunctuation('!'));
  EXPECT_TRUE(unilib_.IsPunctuation('?'));
  EXPECT_TRUE(unilib_.IsPunctuation('#'));
  EXPECT_TRUE(unilib_.IsPunctuation('('));
  EXPECT_FALSE(unilib_.IsPunctuation('0'));
  EXPECT_FALSE(unilib_.IsPunctuation('$'));
  EXPECT_EQ(unilib_.ToLower('A'), 'a');
  EXPECT_EQ(unilib_.ToLower('Z'), 'z');
  EXPECT_EQ(unilib_.ToLower(')'), ')');
  EXPECT_EQ(unilib_.ToLowerText(UTF8ToUnicodeText("Never gonna give you up."))
                .ToUTF8String(),
            "never gonna give you up.");
  EXPECT_EQ(unilib_.ToUpper('a'), 'A');
  EXPECT_EQ(unilib_.ToUpper('z'), 'Z');
  EXPECT_EQ(unilib_.ToUpper(')'), ')');
  EXPECT_EQ(unilib_.ToUpperText(UTF8ToUnicodeText("Never gonna let you down."))
                .ToUTF8String(),
            "NEVER GONNA LET YOU DOWN.");
  EXPECT_EQ(unilib_.GetPairedBracket(')'), '(');
  EXPECT_EQ(unilib_.GetPairedBracket('}'), '{');
}

TEST_F(UniLibTest, CharacterClassesUnicode) {
  EXPECT_TRUE(unilib_.IsOpeningBracket(0x0F3C));  // TIBET ANG KHANG GYON
  EXPECT_TRUE(unilib_.IsClosingBracket(0x0F3D));  // TIBET ANG KHANG GYAS
  EXPECT_FALSE(unilib_.IsWhitespace(0x23F0));     // ALARM CLOCK
  EXPECT_TRUE(unilib_.IsWhitespace(0x2003));      // EM SPACE
  EXPECT_FALSE(unilib_.IsDigit(0xA619));          // VAI SYMBOL JONG
  EXPECT_TRUE(unilib_.IsDigit(0xA620));           // VAI DIGIT ZERO
  EXPECT_TRUE(unilib_.IsDigit(0xA629));           // VAI DIGIT NINE
  EXPECT_FALSE(unilib_.IsDigit(0xA62A));          // VAI SYLLABLE NDOLE MA
  EXPECT_FALSE(unilib_.IsUpper(0x0211));          // SMALL R WITH DOUBLE GRAVE
  EXPECT_TRUE(unilib_.IsUpper(0x0212));           // CAPITAL R WITH DOUBLE GRAVE
  EXPECT_TRUE(unilib_.IsUpper(0x0391));           // GREEK CAPITAL ALPHA
  EXPECT_TRUE(unilib_.IsUpper(0x03AB));         // GREEK CAPITAL UPSILON W DIAL
  EXPECT_FALSE(unilib_.IsUpper(0x03AC));        // GREEK SMALL ALPHA WITH TONOS
  EXPECT_TRUE(unilib_.IsLower(0x03AC));         // GREEK SMALL ALPHA WITH TONOS
  EXPECT_TRUE(unilib_.IsLower(0x03B1));         // GREEK SMALL ALPHA
  EXPECT_TRUE(unilib_.IsLower(0x03CB));         // GREEK SMALL UPSILON
  EXPECT_TRUE(unilib_.IsLower(0x0211));         // SMALL R WITH DOUBLE GRAVE
  EXPECT_TRUE(unilib_.IsLower(0x03C0));         // GREEK SMALL PI
  EXPECT_TRUE(unilib_.IsLower(0x007A));         // SMALL Z
  EXPECT_FALSE(unilib_.IsLower(0x005A));        // CAPITAL Z
  EXPECT_FALSE(unilib_.IsLower(0x0212));        // CAPITAL R WITH DOUBLE GRAVE
  EXPECT_FALSE(unilib_.IsLower(0x0391));        // GREEK CAPITAL ALPHA
  EXPECT_TRUE(unilib_.IsPunctuation(0x055E));   // ARMENIAN QUESTION MARK
  EXPECT_TRUE(unilib_.IsPunctuation(0x066C));   // ARABIC THOUSANDS SEPARATOR
  EXPECT_TRUE(unilib_.IsPunctuation(0x07F7));   // NKO SYMBOL GBAKURUNEN
  EXPECT_TRUE(unilib_.IsPunctuation(0x10AF2));  // DOUBLE DOT WITHIN DOT
  EXPECT_FALSE(unilib_.IsPunctuation(0x00A3));  // POUND SIGN
  EXPECT_FALSE(unilib_.IsPunctuation(0xA838));  // NORTH INDIC RUPEE MARK
  EXPECT_EQ(unilib_.ToLower(0x0391), 0x03B1);   // GREEK ALPHA
  EXPECT_EQ(unilib_.ToLower(0x03AB), 0x03CB);   // GREEK UPSILON WITH DIALYTIKA
  EXPECT_EQ(unilib_.ToLower(0x03C0), 0x03C0);   // GREEK SMALL PI
  EXPECT_EQ(unilib_.ToLower(0x03A3), 0x03C3);   // GREEK CAPITAL LETTER SIGMA
  EXPECT_EQ(unilib_.ToLowerText(UTF8ToUnicodeText("Κανένας άνθρωπος δεν ξέρει"))
                .ToUTF8String(),
            "κανένας άνθρωπος δεν ξέρει");
  EXPECT_EQ(unilib_.ToUpper(0x03B1), 0x0391);  // GREEK ALPHA
  EXPECT_EQ(unilib_.ToUpper(0x03CB), 0x03AB);  // GREEK UPSILON WITH DIALYTIKA
  EXPECT_EQ(unilib_.ToUpper(0x0391), 0x0391);  // GREEK CAPITAL ALPHA
  EXPECT_EQ(unilib_.ToUpper(0x03C3), 0x03A3);  // GREEK CAPITAL LETTER SIGMA
  EXPECT_EQ(unilib_.ToUpper(0x03C2), 0x03A3);  // GREEK CAPITAL LETTER SIGMA
  EXPECT_EQ(unilib_.ToUpperText(UTF8ToUnicodeText("Κανένας άνθρωπος δεν ξέρει"))
                .ToUTF8String(),
            "ΚΑΝΈΝΑΣ ΆΝΘΡΩΠΟΣ ΔΕΝ ΞΈΡΕΙ");
  EXPECT_EQ(unilib_.GetPairedBracket(0x0F3C), 0x0F3D);
  EXPECT_EQ(unilib_.GetPairedBracket(0x0F3D), 0x0F3C);
}

TEST_F(UniLibTest, RegexInterface) {
  const UnicodeText regex_pattern =
      UTF8ToUnicodeText("[0-9]+", /*do_copy=*/true);
  std::unique_ptr<UniLib::RegexPattern> pattern =
      unilib_.CreateRegexPattern(regex_pattern);
  const UnicodeText input = UTF8ToUnicodeText("hello 0123", /*do_copy=*/false);
  int status;
  std::unique_ptr<UniLib::RegexMatcher> matcher = pattern->Matcher(input);
  TC3_LOG(INFO) << matcher->Matches(&status);
  TC3_LOG(INFO) << matcher->Find(&status);
  TC3_LOG(INFO) << matcher->Start(0, &status);
  TC3_LOG(INFO) << matcher->End(0, &status);
  TC3_LOG(INFO) << matcher->Group(0, &status).size_codepoints();
}

TEST_F(UniLibTest, Regex) {
  // The smiley face is a 4-byte UTF8 codepoint 0x1F60B, and it's important to
  // test the regex functionality with it to verify we are handling the indices
  // correctly.
  const UnicodeText regex_pattern =
      UTF8ToUnicodeText("[0-9]+😋", /*do_copy=*/false);
  std::unique_ptr<UniLib::RegexPattern> pattern =
      unilib_.CreateRegexPattern(regex_pattern);
  int status;
  std::unique_ptr<UniLib::RegexMatcher> matcher;

  matcher = pattern->Matcher(UTF8ToUnicodeText("0123😋", /*do_copy=*/false));
  EXPECT_TRUE(matcher->Matches(&status));
  EXPECT_TRUE(matcher->ApproximatelyMatches(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_TRUE(matcher->Matches(&status));  // Check that the state is reset.
  EXPECT_TRUE(matcher->ApproximatelyMatches(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);

  matcher = pattern->Matcher(
      UTF8ToUnicodeText("hello😋😋 0123😋 world", /*do_copy=*/false));
  EXPECT_FALSE(matcher->Matches(&status));
  EXPECT_FALSE(matcher->ApproximatelyMatches(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);

  matcher = pattern->Matcher(
      UTF8ToUnicodeText("hello😋😋 0123😋 world", /*do_copy=*/false));
  EXPECT_TRUE(matcher->Find(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Start(0, &status), 8);
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->End(0, &status), 13);
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Group(0, &status).ToUTF8String(), "0123😋");
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
}

TEST_F(UniLibTest, RegexLazy) {
  std::unique_ptr<UniLib::RegexPattern> pattern =
      unilib_.CreateLazyRegexPattern(
          UTF8ToUnicodeText("[a-z][0-9]", /*do_copy=*/false));
  int status;
  std::unique_ptr<UniLib::RegexMatcher> matcher;

  matcher = pattern->Matcher(UTF8ToUnicodeText("a3", /*do_copy=*/false));
  EXPECT_TRUE(matcher->Matches(&status));
  EXPECT_TRUE(matcher->ApproximatelyMatches(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_TRUE(matcher->Matches(&status));  // Check that the state is reset.
  EXPECT_TRUE(matcher->ApproximatelyMatches(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);

  matcher = pattern->Matcher(UTF8ToUnicodeText("3a", /*do_copy=*/false));
  EXPECT_FALSE(matcher->Matches(&status));
  EXPECT_FALSE(matcher->ApproximatelyMatches(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
}

TEST_F(UniLibTest, RegexGroups) {
  // The smiley face is a 4-byte UTF8 codepoint 0x1F60B, and it's important to
  // test the regex functionality with it to verify we are handling the indices
  // correctly.
  const UnicodeText regex_pattern =
      UTF8ToUnicodeText("([0-9])([0-9]+)😋", /*do_copy=*/false);
  std::unique_ptr<UniLib::RegexPattern> pattern =
      unilib_.CreateRegexPattern(regex_pattern);
  int status;
  std::unique_ptr<UniLib::RegexMatcher> matcher;

  matcher = pattern->Matcher(
      UTF8ToUnicodeText("hello😋😋 0123😋 world", /*do_copy=*/false));
  EXPECT_TRUE(matcher->Find(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Start(0, &status), 8);
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Start(1, &status), 8);
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Start(2, &status), 9);
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->End(0, &status), 13);
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->End(1, &status), 9);
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->End(2, &status), 12);
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Group(0, &status).ToUTF8String(), "0123😋");
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Group(1, &status).ToUTF8String(), "0");
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Group(2, &status).ToUTF8String(), "123");
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
}

TEST_F(UniLibTest, RegexGroupsNotAllGroupsInvolved) {
  const UnicodeText regex_pattern =
      UTF8ToUnicodeText("([0-9])([a-z])?", /*do_copy=*/false);
  std::unique_ptr<UniLib::RegexPattern> pattern =
      unilib_.CreateRegexPattern(regex_pattern);
  int status;
  std::unique_ptr<UniLib::RegexMatcher> matcher;

  matcher = pattern->Matcher(UTF8ToUnicodeText("7", /*do_copy=*/false));
  EXPECT_TRUE(matcher->Find(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Group(0, &status).ToUTF8String(), "7");
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Group(1, &status).ToUTF8String(), "7");
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Group(2, &status).ToUTF8String(), "");
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
}

TEST_F(UniLibTest, RegexGroupsEmptyResult) {
  const UnicodeText regex_pattern =
      UTF8ToUnicodeText("(.*)", /*do_copy=*/false);
  std::unique_ptr<UniLib::RegexPattern> pattern =
      unilib_.CreateRegexPattern(regex_pattern);
  int status;
  std::unique_ptr<UniLib::RegexMatcher> matcher;

  matcher = pattern->Matcher(UTF8ToUnicodeText("", /*do_copy=*/false));
  EXPECT_TRUE(matcher->Find(&status));
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Group(0, &status).ToUTF8String(), "");
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
  EXPECT_EQ(matcher->Group(1, &status).ToUTF8String(), "");
  EXPECT_EQ(status, UniLib::RegexMatcher::kNoError);
}

TEST_F(UniLibTest, BreakIterator) {
  const UnicodeText text = UTF8ToUnicodeText("some text", /*do_copy=*/false);
  std::unique_ptr<UniLib::BreakIterator> iterator =
      unilib_.CreateBreakIterator(text);
  std::vector<int> break_indices;
  int break_index = 0;
  while ((break_index = iterator->Next()) != UniLib::BreakIterator::kDone) {
    break_indices.push_back(break_index);
  }
  EXPECT_THAT(break_indices, ElementsAre(4, 5, 9));
}

TEST_F(UniLibTest, BreakIterator4ByteUTF8) {
  const UnicodeText text = UTF8ToUnicodeText("😀😂😋", /*do_copy=*/false);
  std::unique_ptr<UniLib::BreakIterator> iterator =
      unilib_.CreateBreakIterator(text);
  std::vector<int> break_indices;
  int break_index = 0;
  while ((break_index = iterator->Next()) != UniLib::BreakIterator::kDone) {
    break_indices.push_back(break_index);
  }
  EXPECT_THAT(break_indices, ElementsAre(1, 2, 3));
}

TEST_F(UniLibTest, IntegerParse) {
  int result;
  EXPECT_TRUE(
      unilib_.ParseInt32(UTF8ToUnicodeText("123", /*do_copy=*/false), &result));
  EXPECT_EQ(result, 123);
}

TEST_F(UniLibTest, IntegerParseFullWidth) {
  int result;
  // The input string here is full width
  EXPECT_TRUE(unilib_.ParseInt32(UTF8ToUnicodeText("１２３", /*do_copy=*/false),
                                 &result));
  EXPECT_EQ(result, 123);
}

TEST_F(UniLibTest, IntegerParseFullWidthWithAlpha) {
  int result;
  // The input string here is full width
  EXPECT_FALSE(unilib_.ParseInt32(UTF8ToUnicodeText("１a３", /*do_copy=*/false),
                                  &result));
}
}  // namespace test_internal
}  // namespace libtextclassifier3
