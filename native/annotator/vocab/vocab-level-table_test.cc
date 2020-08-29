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

#include "annotator/vocab/vocab-level-table.h"

#include <memory>

#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/test-data-test-utils.h"
#include "utils/test-utils.h"
#include "utils/testing/annotator.h"
#include "utils/utf8/unicodetext.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

class VocabLevelTableTest : public testing::Test {
 protected:
  void SetUp() override {
    std::string test_model_path =
        GetTestDataPath("annotator/vocab/test_data/test.model");
    model_buffer_ = ReadFile(test_model_path);
    annotator_model_ = GetModel(model_buffer_.data());
    vocab_table_ = VocabLevelTable::Create(annotator_model_->vocab_model());
  }

  std::string model_buffer_;
  const Model* annotator_model_;
  std::unique_ptr<VocabLevelTable> vocab_table_;
};

TEST_F(VocabLevelTableTest, Lookup_Exists) {
  LookupResult result = vocab_table_->Lookup("apple").value();
  EXPECT_EQ(result.beginner_level, 1);
  EXPECT_EQ(result.do_not_trigger_in_upper_case, 1);

  result = vocab_table_->Lookup("easy").value();
  EXPECT_EQ(result.beginner_level, 1);
  EXPECT_EQ(result.do_not_trigger_in_upper_case, 0);

  result = vocab_table_->Lookup("johnson").value();
  EXPECT_EQ(result.beginner_level, 0);
  EXPECT_EQ(result.do_not_trigger_in_upper_case, 1);

  result = vocab_table_->Lookup("picturesque").value();
  EXPECT_EQ(result.beginner_level, 0);
  EXPECT_EQ(result.do_not_trigger_in_upper_case, 0);
}

TEST_F(VocabLevelTableTest, Lookup_NotExists) {
  EXPECT_FALSE(vocab_table_->Lookup("thisworddoesnotexist").has_value());
}

}  // namespace
}  // namespace libtextclassifier3
