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

#include "smartselect/text-classification-model.h"

#include <fcntl.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "gtest/gtest.h"

namespace libtextclassifier {
namespace {

std::string ReadFile(const std::string& file_name) {
  std::ifstream file_stream(file_name);
  return std::string(std::istreambuf_iterator<char>(file_stream), {});
}

std::string GetModelPath() {
  return TEST_DATA_DIR "smartselection.model";
}

std::string GetURLRegexPath() {
  return TEST_DATA_DIR "regex_url.txt";
}

std::string GetEmailRegexPath() {
  return TEST_DATA_DIR "regex_email.txt";
}

TEST(TextClassificationModelTest, StripUnpairedBrackets) {
  // Stripping brackets strip brackets from length 1 bracket only selections.
  EXPECT_EQ(StripUnpairedBrackets("call me at ) today", {11, 12}),
            std::make_pair(12, 12));
  EXPECT_EQ(StripUnpairedBrackets("call me at ( today", {11, 12}),
            std::make_pair(12, 12));
}

TEST(TextClassificationModelTest, ReadModelOptions) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  ModelOptions model_options;
  ASSERT_TRUE(ReadSelectionModelOptions(fd, &model_options));
  close(fd);

  EXPECT_EQ("en", model_options.language());
  EXPECT_GT(model_options.version(), 0);
}

TEST(TextClassificationModelTest, SuggestSelection) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TextClassificationModel> model(
      new TextClassificationModel(fd));
  close(fd);

  EXPECT_EQ(model->SuggestSelection(
                "this afternoon Barack Obama gave a speech at", {15, 21}),
            std::make_pair(15, 27));

  // Try passing whole string.
  // If more than 1 token is specified, we should return back what entered.
  EXPECT_EQ(model->SuggestSelection("350 Third Street, Cambridge", {0, 27}),
            std::make_pair(0, 27));

  // Single letter.
  EXPECT_EQ(std::make_pair(0, 1), model->SuggestSelection("a", {0, 1}));

  // Single word.
  EXPECT_EQ(std::make_pair(0, 4), model->SuggestSelection("asdf", {0, 4}));

  EXPECT_EQ(model->SuggestSelection("call me at 857 225 3556 today", {11, 14}),
            std::make_pair(11, 23));

  // Unpaired bracket stripping.
  EXPECT_EQ(
      model->SuggestSelection("call me at (857) 225 3556 today", {11, 16}),
      std::make_pair(11, 25));
  EXPECT_EQ(model->SuggestSelection("call me at (857 225 3556 today", {11, 15}),
            std::make_pair(12, 24));
  EXPECT_EQ(model->SuggestSelection("call me at 857 225 3556) today", {11, 14}),
            std::make_pair(11, 23));
  EXPECT_EQ(
      model->SuggestSelection("call me at )857 225 3556( today", {11, 15}),
      std::make_pair(12, 24));

  // If the resulting selection would be empty, the original span is returned.
  EXPECT_EQ(model->SuggestSelection("call me at )( today", {11, 13}),
            std::make_pair(11, 13));
  EXPECT_EQ(model->SuggestSelection("call me at ( today", {11, 12}),
            std::make_pair(11, 12));
  EXPECT_EQ(model->SuggestSelection("call me at ) today", {11, 12}),
            std::make_pair(11, 12));
}

TEST(TextClassificationModelTest, SuggestSelectionsAreSymmetric) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TextClassificationModel> model(
      new TextClassificationModel(fd));
  close(fd);

  EXPECT_EQ(std::make_pair(0, 27),
            model->SuggestSelection("350 Third Street, Cambridge", {0, 3}));
  EXPECT_EQ(std::make_pair(0, 27),
            model->SuggestSelection("350 Third Street, Cambridge", {4, 9}));
  EXPECT_EQ(std::make_pair(0, 27),
            model->SuggestSelection("350 Third Street, Cambridge", {10, 16}));
  EXPECT_EQ(std::make_pair(6, 33),
            model->SuggestSelection("a\nb\nc\n350 Third Street, Cambridge",
                                    {16, 22}));
}

TEST(TextClassificationModelTest, SuggestSelectionWithNewLine) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TextClassificationModel> model(
      new TextClassificationModel(fd));
  close(fd);

  std::tuple<int, int> selection;
  selection = model->SuggestSelection("abc\nBarack Obama", {4, 10});
  EXPECT_EQ(4, std::get<0>(selection));
  EXPECT_EQ(16, std::get<1>(selection));

  selection = model->SuggestSelection("Barack Obama\nabc", {0, 6});
  EXPECT_EQ(0, std::get<0>(selection));
  EXPECT_EQ(12, std::get<1>(selection));
}

TEST(TextClassificationModelTest, SuggestSelectionWithPunctuation) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TextClassificationModel> model(
      new TextClassificationModel(fd));
  close(fd);

  std::tuple<int, int> selection;

  // From the right.
  selection = model->SuggestSelection(
      "this afternoon Barack Obama, gave a speech at", {15, 21});
  EXPECT_EQ(15, std::get<0>(selection));
  EXPECT_EQ(27, std::get<1>(selection));

  // From the right multiple.
  selection = model->SuggestSelection(
      "this afternoon Barack Obama,.,.,, gave a speech at", {15, 21});
  EXPECT_EQ(15, std::get<0>(selection));
  EXPECT_EQ(27, std::get<1>(selection));

  // From the left multiple.
  selection = model->SuggestSelection(
      "this afternoon ,.,.,,Barack Obama gave a speech at", {21, 27});
  EXPECT_EQ(21, std::get<0>(selection));
  EXPECT_EQ(27, std::get<1>(selection));

  // From both sides.
  selection = model->SuggestSelection(
      "this afternoon !Barack Obama,- gave a speech at", {16, 22});
  EXPECT_EQ(16, std::get<0>(selection));
  EXPECT_EQ(28, std::get<1>(selection));
}

class TestingTextClassificationModel
    : public libtextclassifier::TextClassificationModel {
 public:
  explicit TestingTextClassificationModel(int fd)
      : libtextclassifier::TextClassificationModel(fd) {}

  using TextClassificationModel::InitializeSharingRegexPatterns;

  void DisableClassificationHints() {
    sharing_options_.set_always_accept_url_hint(false);
    sharing_options_.set_always_accept_email_hint(false);
  }
};

TEST(TextClassificationModelTest, SuggestSelectionNoCrashWithJunk) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TextClassificationModel> ff_model(
      new TextClassificationModel(fd));
  close(fd);

  std::tuple<int, int> selection;

  // Try passing in bunch of invalid selections.
  selection = ff_model->SuggestSelection("", {0, 27});
  // If more than 1 token is specified, we should return back what entered.
  EXPECT_EQ(0, std::get<0>(selection));
  EXPECT_EQ(27, std::get<1>(selection));

  selection = ff_model->SuggestSelection("", {-10, 27});
  // If more than 1 token is specified, we should return back what entered.
  EXPECT_EQ(-10, std::get<0>(selection));
  EXPECT_EQ(27, std::get<1>(selection));

  selection = ff_model->SuggestSelection("Word 1 2 3 hello!", {0, 27});
  // If more than 1 token is specified, we should return back what entered.
  EXPECT_EQ(0, std::get<0>(selection));
  EXPECT_EQ(27, std::get<1>(selection));

  selection = ff_model->SuggestSelection("Word 1 2 3 hello!", {-30, 300});
  // If more than 1 token is specified, we should return back what entered.
  EXPECT_EQ(-30, std::get<0>(selection));
  EXPECT_EQ(300, std::get<1>(selection));

  selection = ff_model->SuggestSelection("Word 1 2 3 hello!", {-10, -1});
  // If more than 1 token is specified, we should return back what entered.
  EXPECT_EQ(-10, std::get<0>(selection));
  EXPECT_EQ(-1, std::get<1>(selection));

  selection = ff_model->SuggestSelection("Word 1 2 3 hello!", {100, 17});
  // If more than 1 token is specified, we should return back what entered.
  EXPECT_EQ(100, std::get<0>(selection));
  EXPECT_EQ(17, std::get<1>(selection));
}

namespace {

std::string FindBestResult(std::vector<std::pair<std::string, float>> results) {
  if (results.empty()) {
    return "<INVALID RESULTS>";
  }

  std::sort(results.begin(), results.end(),
            [](const std::pair<std::string, float> a,
               const std::pair<std::string, float> b) {
              return a.second > b.second;
            });
  return results[0].first;
}

}  // namespace

TEST(TextClassificationModelTest, ClassifyText) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TestingTextClassificationModel> model(
      new TestingTextClassificationModel(fd));
  close(fd);

  model->DisableClassificationHints();
  EXPECT_EQ("other",
            FindBestResult(model->ClassifyText(
                "this afternoon Barack Obama gave a speech at", {15, 27})));
  EXPECT_EQ("other",
            FindBestResult(model->ClassifyText("you@android.com", {0, 15})));
  EXPECT_EQ("other", FindBestResult(model->ClassifyText(
                         "Contact me at you@android.com", {14, 29})));
  EXPECT_EQ("phone", FindBestResult(model->ClassifyText(
                         "Call me at (800) 123-456 today", {11, 24})));
  EXPECT_EQ("other", FindBestResult(model->ClassifyText(
                         "Visit www.google.com every today!", {6, 20})));

  // More lines.
  EXPECT_EQ("other",
            FindBestResult(model->ClassifyText(
                "this afternoon Barack Obama gave a speech at|Visit "
                "www.google.com every today!|Call me at (800) 123-456 today.",
                {15, 27})));
  EXPECT_EQ("other",
            FindBestResult(model->ClassifyText(
                "this afternoon Barack Obama gave a speech at|Visit "
                "www.google.com every today!|Call me at (800) 123-456 today.",
                {51, 65})));
  EXPECT_EQ("phone",
            FindBestResult(model->ClassifyText(
                "this afternoon Barack Obama gave a speech at|Visit "
                "www.google.com every today!|Call me at (800) 123-456 today.",
                {90, 103})));

  // Single word.
  EXPECT_EQ("other", FindBestResult(model->ClassifyText("obama", {0, 5})));
  EXPECT_EQ("other", FindBestResult(model->ClassifyText("asdf", {0, 4})));
  EXPECT_EQ("<INVALID RESULTS>",
            FindBestResult(model->ClassifyText("asdf", {0, 0})));

  // Junk.
  EXPECT_EQ("<INVALID RESULTS>",
            FindBestResult(model->ClassifyText("", {0, 0})));
  EXPECT_EQ("<INVALID RESULTS>", FindBestResult(model->ClassifyText(
                                     "a\n\n\n\nx x x\n\n\n\n\n\n", {1, 5})));
}

TEST(TextClassificationModelTest, ClassifyTextWithHints) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TestingTextClassificationModel> model(
      new TestingTextClassificationModel(fd));
  close(fd);

  // When EMAIL hint is passed, the result should be email.
  EXPECT_EQ("email",
            FindBestResult(model->ClassifyText(
                "x", {0, 1}, TextClassificationModel::SELECTION_IS_EMAIL)));
  // When URL hint is passed, the result should be email.
  EXPECT_EQ("url",
            FindBestResult(model->ClassifyText(
                "x", {0, 1}, TextClassificationModel::SELECTION_IS_URL)));
  // When both hints are passed, the result should be url (as it's probably
  // better to let Chrome handle this case).
  EXPECT_EQ("url", FindBestResult(model->ClassifyText(
                       "x", {0, 1},
                       TextClassificationModel::SELECTION_IS_EMAIL |
                           TextClassificationModel::SELECTION_IS_URL)));

  // With disabled hints, we should get the same prediction regardless of the
  // hint.
  model->DisableClassificationHints();
  EXPECT_EQ(model->ClassifyText("x", {0, 1}, 0),
            model->ClassifyText("x", {0, 1},
                                TextClassificationModel::SELECTION_IS_EMAIL));

  EXPECT_EQ(model->ClassifyText("x", {0, 1}, 0),
            model->ClassifyText("x", {0, 1},
                                TextClassificationModel::SELECTION_IS_URL));
}

TEST(TextClassificationModelTest, PhoneFiltering) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TestingTextClassificationModel> model(
      new TestingTextClassificationModel(fd));
  close(fd);

  EXPECT_EQ("phone", FindBestResult(model->ClassifyText("phone: (123) 456 789",
                                                        {7, 20}, 0)));
  EXPECT_EQ("phone", FindBestResult(model->ClassifyText(
                         "phone: (123) 456 789,0001112", {7, 25}, 0)));
  EXPECT_EQ("other", FindBestResult(model->ClassifyText(
                         "phone: (123) 456 789,0001112", {7, 28}, 0)));
}

TEST(TextClassificationModelTest, Annotate) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TestingTextClassificationModel> model(
      new TestingTextClassificationModel(fd));
  close(fd);

  std::string test_string =
      "& saw Barak Obama today .. 350 Third Street, Cambridge\nand my phone "
      "number is 853 225-3556.";
  std::vector<TextClassificationModel::AnnotatedSpan> result =
      model->Annotate(test_string);

  std::vector<TextClassificationModel::AnnotatedSpan> expected;
  expected.emplace_back();
  expected.back().span = {0, 0};
  expected.emplace_back();
  expected.back().span = {2, 5};
  expected.back().classification.push_back({"other", 1.0});
  expected.emplace_back();
  expected.back().span = {6, 17};
  expected.back().classification.push_back({"other", 1.0});
  expected.emplace_back();
  expected.back().span = {18, 23};
  expected.back().classification.push_back({"other", 1.0});
  expected.emplace_back();
  expected.back().span = {24, 24};
  expected.emplace_back();
  expected.back().span = {27, 54};
  expected.back().classification.push_back({"address", 1.0});
  expected.emplace_back();
  expected.back().span = {55, 58};
  expected.back().classification.push_back({"other", 1.0});
  expected.emplace_back();
  expected.back().span = {59, 61};
  expected.back().classification.push_back({"other", 1.0});
  expected.emplace_back();
  expected.back().span = {62, 74};
  expected.back().classification.push_back({"other", 1.0});
  expected.emplace_back();
  expected.back().span = {75, 77};
  expected.back().classification.push_back({"other", 1.0});
  expected.emplace_back();
  expected.back().span = {78, 90};
  expected.back().classification.push_back({"phone", 1.0});

  EXPECT_EQ(result.size(), expected.size());
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(result[i].span, expected[i].span) << result[i];
    if (!expected[i].classification.empty()) {
      EXPECT_GT(result[i].classification.size(), 0);
      EXPECT_EQ(result[i].classification[0].first,
                expected[i].classification[0].first)
          << result[i];
    }
  }
}

TEST(TextClassificationModelTest, URLEmailRegex) {
  const std::string model_path = GetModelPath();
  int fd = open(model_path.c_str(), O_RDONLY);
  std::unique_ptr<TestingTextClassificationModel> model(
      new TestingTextClassificationModel(fd));
  close(fd);

  SharingModelOptions options;
  SharingModelOptions::RegexPattern* email_pattern =
      options.add_regex_pattern();
  email_pattern->set_collection_name("email");
  email_pattern->set_pattern(ReadFile(GetEmailRegexPath()));
  SharingModelOptions::RegexPattern* url_pattern = options.add_regex_pattern();
  url_pattern->set_collection_name("url");
  url_pattern->set_pattern(ReadFile(GetURLRegexPath()));

  // TODO(b/69538802): Modify directly the model image instead.
  model->InitializeSharingRegexPatterns(
      {options.regex_pattern().begin(), options.regex_pattern().end()});

  EXPECT_EQ("url", FindBestResult(model->ClassifyText(
                       "Visit www.google.com every today!", {6, 20})));
  EXPECT_EQ("email", FindBestResult(model->ClassifyText(
                         "My email: asdf@something.cz", {10, 27})));
  EXPECT_EQ("url", FindBestResult(model->ClassifyText(
                       "Login: http://asdf@something.cz", {7, 31})));
}

}  // namespace
}  // namespace libtextclassifier
