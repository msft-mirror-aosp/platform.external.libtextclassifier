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

#include "annotator/grammar/grammar-annotator.h"

#include "annotator/model_generated.h"
#include "utils/grammar/utils/rules.h"
#include "utils/tokenizer.h"
#include "utils/utf8/unilib.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using testing::ElementsAre;
using testing::Pair;
using testing::Value;

MATCHER_P3(IsAnnotatedSpan, start, end, collection, "") {
  return Value(arg.span, Pair(start, end)) &&
         Value(arg.classification.front().collection, collection);
}

class GrammarAnnotatorTest : public testing::Test {
 protected:
  GrammarAnnotatorTest()
      : INIT_UNILIB_FOR_TESTING(unilib_),
        tokenizer_(TokenizationType_ICU, &unilib_, {}, {},
                   /*split_on_script_change=*/true,
                   /*icu_preserve_whitespace_tokens=*/true) {}

  flatbuffers::DetachedBuffer PackModel(const GrammarModelT& model) const {
    flatbuffers::FlatBufferBuilder builder;
    builder.Finish(GrammarModel::Pack(builder, &model));
    return builder.Release();
  }

  int AddRuleClassificationResult(const std::string& collection,
                                  GrammarModelT* model) const {
    const int result_id = model->rule_classification_result.size();
    model->rule_classification_result.emplace_back(
        new GrammarModel_::RuleClassificationResultT);
    GrammarModel_::RuleClassificationResultT* result =
        model->rule_classification_result.back().get();
    result->collection_name = collection;
    return result_id;
  }

  UniLib unilib_;
  Tokenizer tokenizer_;
};

TEST_F(GrammarAnnotatorTest, AnnotesWithGrammarRules) {
  // Create test rules.
  GrammarModelT grammar_model;
  grammar_model.rules.reset(new grammar::RulesSetT);
  grammar::Rules rules;
  rules.Add("<carrier>", {"lx"});
  rules.Add("<carrier>", {"aa"});
  rules.Add("<flight_code>", {"<2_digits>"});
  rules.Add("<flight_code>", {"<3_digits>"});
  rules.Add("<flight_code>", {"<4_digits>"});
  rules.Add(
      "<flight>", {"<carrier>", "<flight_code>"},
      /*callback=*/
      static_cast<grammar::CallbackId>(GrammarAnnotator::Callback::kRuleMatch),
      /*callback_param=*/
      AddRuleClassificationResult("flight", &grammar_model));
  rules.Finalize().Serialize(/*include_debug_information=*/false,
                             grammar_model.rules.get());
  flatbuffers::DetachedBuffer serialized_model = PackModel(grammar_model);
  GrammarAnnotator annotator(
      &unilib_, flatbuffers::GetRoot<GrammarModel>(serialized_model.data()));

  std::vector<AnnotatedSpan> result;
  EXPECT_TRUE(annotator.Annotate(
      {Locale::FromBCP47("en")},
      tokenizer_.Tokenize(
          "My flight: LX 38 arriving at 4pm, I'll fly back on AA2014"),
      &result));

  EXPECT_THAT(result, ElementsAre(IsAnnotatedSpan(11, 16, "flight"),
                                  IsAnnotatedSpan(51, 57, "flight")));
}

TEST_F(GrammarAnnotatorTest, HandlesAssertions) {
  // Create test rules.
  GrammarModelT grammar_model;
  grammar_model.rules.reset(new grammar::RulesSetT);
  grammar::Rules rules;
  rules.Add("<carrier>", {"lx"});
  rules.Add("<carrier>", {"aa"});
  rules.Add("<flight_code>", {"<2_digits>"});
  rules.Add("<flight_code>", {"<3_digits>"});
  rules.Add("<flight_code>", {"<4_digits>"});

  // Flight: carrier + flight code and check right context.
  rules.Add(
      "<flight>", {"<carrier>", "<flight_code>", "<context_assertion>?"},
      /*callback=*/
      static_cast<grammar::CallbackId>(GrammarAnnotator::Callback::kRuleMatch),
      /*callback_param=*/
      AddRuleClassificationResult("flight", &grammar_model));

  // Exclude matches like: LX 38.00 etc.
  rules.Add("<context_assertion>", {".?", "<digits>"},
            /*callback=*/
            static_cast<grammar::CallbackId>(
                GrammarAnnotator::Callback::kAssertionMatch),
            /*callback_param=*/true /*negative*/);

  // Assertion matches will create their own match objects.
  // We declare the handler as a filter so that the grammar system knows that we
  // handle this ourselves.
  rules.DefineFilter(static_cast<grammar::CallbackId>(
      GrammarAnnotator::Callback::kAssertionMatch));

  rules.Finalize().Serialize(/*include_debug_information=*/false,
                             grammar_model.rules.get());
  flatbuffers::DetachedBuffer serialized_model = PackModel(grammar_model);
  GrammarAnnotator annotator(
      &unilib_, flatbuffers::GetRoot<GrammarModel>(serialized_model.data()));

  std::vector<AnnotatedSpan> result;
  EXPECT_TRUE(annotator.Annotate(
      {Locale::FromBCP47("en")},
      tokenizer_.Tokenize("My flight: LX 38 arriving at 4pm, I'll fly back on "
                          "AA2014 on LX 38.00"),
      &result));

  EXPECT_THAT(result, ElementsAre(IsAnnotatedSpan(11, 16, "flight"),
                                  IsAnnotatedSpan(51, 57, "flight")));
}

TEST_F(GrammarAnnotatorTest, HandlesCapturingGroups) {
  // Create test rules.
  GrammarModelT grammar_model;
  grammar_model.rules.reset(new grammar::RulesSetT);
  grammar::Rules rules;
  rules.Add("<low_confidence_phone>", {"<digits>"},
            /*callback=*/
            static_cast<grammar::CallbackId>(
                GrammarAnnotator::Callback::kCapturingMatch),
            /*callback_param=*/0);

  // Create rule result.
  const int classification_result_id =
      AddRuleClassificationResult("phone", &grammar_model);
  grammar_model.rule_classification_result[classification_result_id]
      ->capturing_group.emplace_back(new CapturingGroupT);
  grammar_model.rule_classification_result[classification_result_id]
      ->capturing_group.back()
      ->extend_selection = true;

  rules.Add(
      "<phone>", {"please", "call", "<low_confidence_phone>"},
      /*callback=*/
      static_cast<grammar::CallbackId>(GrammarAnnotator::Callback::kRuleMatch),
      /*callback_param=*/classification_result_id);

  // Capturing matches will create their own match objects to keep track of
  // match ids, so we declare the handler as a filter so that the grammar system
  // knows that we handle this ourselves.
  rules.DefineFilter(static_cast<grammar::CallbackId>(
      GrammarAnnotator::Callback::kCapturingMatch));

  rules.Finalize().Serialize(/*include_debug_information=*/false,
                             grammar_model.rules.get());
  flatbuffers::DetachedBuffer serialized_model = PackModel(grammar_model);
  GrammarAnnotator annotator(
      &unilib_, flatbuffers::GetRoot<GrammarModel>(serialized_model.data()));

  std::vector<AnnotatedSpan> result;
  EXPECT_TRUE(annotator.Annotate(
      {Locale::FromBCP47("en")},
      tokenizer_.Tokenize("Please call 911 before 10 am!"), &result));
  EXPECT_THAT(result, ElementsAre(IsAnnotatedSpan(12, 15, "phone")));
}

}  // namespace
}  // namespace libtextclassifier3
