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

#include "annotator/vocab/vocab-annotator-impl.h"

#include <memory>

#include "annotator/feature-processor.h"
#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/i18n/locale.h"
#include "utils/jvm-test-utils.h"
#include "utils/test-data-test-utils.h"
#include "utils/test-utils.h"
#include "utils/testing/annotator.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

struct CreateAnnotatorResult {
  const std::unique_ptr<libtextclassifier3::FeatureProcessor> feature_processor;
  const std::unique_ptr<std::string> model_buffer;
  const std::unique_ptr<VocabAnnotator> vocab_annotator;
};

class VocabAnnotatorTest : public ::testing::Test {
 protected:
  VocabAnnotatorTest() {}

  ~VocabAnnotatorTest() override {}

  void SetUp() override {
    unilib_ = CreateUniLibForTesting();
    const std::string test_model_path =
        GetTestDataPath("annotator/vocab/test_data/test.model");
    model_buffer_ = ReadFile(test_model_path);
  }

  std::unique_ptr<std::string> CreateModifiedModelBuffer(
      const std::string triggering_locales) const {
    std::unique_ptr<ModelT> unpacked_model = UnPackModel(model_buffer_.c_str());

    if (!triggering_locales.empty()) {
      unpacked_model->vocab_model->triggering_locales = triggering_locales;
    }

    flatbuffers::FlatBufferBuilder builder;
    FinishModelBuffer(builder, Model::Pack(builder, unpacked_model.get()));
    return std::make_unique<std::string>(
        reinterpret_cast<const char*>(builder.GetBufferPointer()),
        builder.GetSize());
  }

  CreateAnnotatorResult CreateAnnotator(
      const std::string triggering_locales) const {
    std::unique_ptr<std::string> buffer =
        CreateModifiedModelBuffer(triggering_locales);
    const Model* annotator_model = GetModel(buffer->data());
    std::unique_ptr<FeatureProcessor> feature_processor =
        std::unique_ptr<libtextclassifier3::FeatureProcessor>(
            new libtextclassifier3::FeatureProcessor(
                annotator_model->classification_feature_options(),
                unilib_.get()));
    std::unique_ptr<VocabAnnotator> vocab_annotator = VocabAnnotator::Create(
        annotator_model->vocab_model(), *feature_processor, *unilib_);
    return {std::move(feature_processor), std::move(buffer),
            std::move(vocab_annotator)};
  }

  void TearDown() override {}

  flatbuffers::DetachedBuffer PackFeatureProcessorOptions(
      const FeatureProcessorOptionsT& options) {
    flatbuffers::FlatBufferBuilder builder;
    builder.Finish(CreateFeatureProcessorOptions(builder, &options));
    return builder.Release();
  }

  std::unique_ptr<UniLib> unilib_;
  std::string model_buffer_;
};

TEST_F(VocabAnnotatorTest, AnnotateSmokeTest) {
  const CreateAnnotatorResult create_annotator_result = CreateAnnotator("en");

  std::vector<AnnotatedSpan> annotations;
  ASSERT_TRUE(create_annotator_result.vocab_annotator->Annotate(
      UTF8ToUnicodeText("The Picturesque streets of the old city."),
      {Locale::FromBCP47("en")},
      /*trigger_on_beginner_words=*/false, &annotations));
  EXPECT_THAT(annotations, Not(IsEmpty()));
}

TEST_F(VocabAnnotatorTest, ClassifyText_Positive) {
  const CreateAnnotatorResult create_annotator_result = CreateAnnotator("en");

  ClassificationResult result;
  ASSERT_TRUE(create_annotator_result.vocab_annotator->ClassifyText(
      UTF8ToUnicodeText("The Picturesque streets of the old city."), {4, 15},
      {Locale::FromBCP47("en")},
      /*trigger_on_beginner_words=*/false, &result));
  EXPECT_EQ(result.collection, "dictionary");
}

TEST_F(VocabAnnotatorTest, ClassifyText_DoNotTriggerInTitleCase) {
  const CreateAnnotatorResult create_annotator_result = CreateAnnotator("en");

  ClassificationResult result;
  ASSERT_FALSE(create_annotator_result.vocab_annotator->ClassifyText(
      UTF8ToUnicodeText("Johnson is having fun."), {0, 7},
      {Locale::FromBCP47("en")},
      /*trigger_on_beginner_words=*/false, &result));
}

TEST_F(VocabAnnotatorTest, ClassifyText_TriggerOnBeginnerWordsFalse) {
  const CreateAnnotatorResult create_annotator_result = CreateAnnotator("en");

  ClassificationResult result;
  ASSERT_FALSE(create_annotator_result.vocab_annotator->ClassifyText(
      UTF8ToUnicodeText("easy"), {0, 4}, {Locale::FromBCP47("en")},
      /*trigger_on_beginner_words=*/false, &result));
}

TEST_F(VocabAnnotatorTest, ClassifyText_TriggerOnBeginnerWordsTrue) {
  const CreateAnnotatorResult create_annotator_result = CreateAnnotator("en");

  ClassificationResult result;
  ASSERT_TRUE(create_annotator_result.vocab_annotator->ClassifyText(
      UTF8ToUnicodeText("easy"), {0, 4}, {Locale::FromBCP47("en")},
      /*trigger_on_beginner_words=*/true, &result));
  EXPECT_EQ(result.collection, "dictionary");
}

TEST_F(VocabAnnotatorTest, ClassifyText_NotInModel) {
  const CreateAnnotatorResult create_annotator_result = CreateAnnotator("en");

  ClassificationResult result;
  ASSERT_FALSE(create_annotator_result.vocab_annotator->ClassifyText(
      UTF8ToUnicodeText("The Picturesque streets of the old city."), {0, 3},
      {Locale::FromBCP47("en")},
      /*trigger_on_beginner_words=*/false, &result));
}

TEST_F(VocabAnnotatorTest, ClassifyText_NoTriggeringLocale) {
  const CreateAnnotatorResult create_annotator_result = CreateAnnotator("");

  ClassificationResult result;
  ASSERT_TRUE(create_annotator_result.vocab_annotator->ClassifyText(
      UTF8ToUnicodeText("The Picturesque streets of the old city."), {4, 15},
      {Locale::FromBCP47("en")},
      /*trigger_on_beginner_words=*/false, &result));
  EXPECT_EQ(result.collection, "dictionary");
}

TEST_F(VocabAnnotatorTest, ClassifyText_LocaleMistmatch) {
  const CreateAnnotatorResult create_annotator_result = CreateAnnotator("en");

  ClassificationResult result;
  ASSERT_FALSE(create_annotator_result.vocab_annotator->ClassifyText(
      UTF8ToUnicodeText("The Picturesque streets of the old city."), {4, 15},
      {Locale::FromBCP47("de")},
      /*trigger_on_beginner_words=*/false, &result));
}

}  // namespace
}  // namespace libtextclassifier3
