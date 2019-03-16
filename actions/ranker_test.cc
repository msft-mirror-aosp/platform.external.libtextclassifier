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

#include "actions/ranker.h"

#include <string>

#include "actions/types.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

MATCHER_P3(IsAction, type, response_text, score, "") {
  return testing::Value(arg.type, type) &&
         testing::Value(arg.response_text, response_text) &&
         testing::Value(arg.score, score);
}

MATCHER_P(IsActionType, type, "") { return testing::Value(arg.type, type); }

TEST(RankingTest, DeduplicationSmartReply) {
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0},
      {/*response_text=*/"hello there", /*type=*/"text_reply", /*score=*/0.5}};

  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()));

  ranker->RankActions(&response);
  EXPECT_THAT(
      response.actions,
      testing::ElementsAreArray({IsAction("text_reply", "hello there", 1.0)}));
}

TEST(RankingTest, DeduplicationExtraData) {
  ActionsSuggestionsResponse response;
  response.actions = {
      {/*response_text=*/"hello there", /*type=*/"text_reply",
       /*score=*/1.0, /*priority_score=*/0.0},
      {/*response_text=*/"hello there", /*type=*/"text_reply", /*score=*/0.5,
       /*priority_score=*/0.0},
      {/*response_text=*/"hello there", /*type=*/"text_reply", /*score=*/0.6,
       /*priority_score=*/0.0,
       /*annotations=*/{}, /*serialized_entity_data=*/"test"},
  };

  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()));

  ranker->RankActions(&response);
  EXPECT_THAT(
      response.actions,
      testing::ElementsAreArray({IsAction("text_reply", "hello there", 1.0),
                                 // Is kept as it has different entity data.
                                 IsAction("text_reply", "hello there", 0.6)}));
}

TEST(RankingTest, DeduplicationAnnotations) {
  ActionsSuggestionsResponse response;
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{0, 21},
                       /*text=*/"742 Evergreen Terrace"};
    annotation.entity = ClassificationResult("address", 0.5);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"view_map",
                                /*score=*/0.5,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{0, 21},
                       /*text=*/"742 Evergreen Terrace"};
    annotation.entity = ClassificationResult("address", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"view_map",
                                /*score=*/1.0,
                                /*priority_score=*/2.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{40, 53},
                       /*text=*/"1-800-TESTING"};
    annotation.entity = ClassificationResult("phone", 0.5);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"call_phone",
                                /*score=*/0.5,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }

  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()));

  ranker->RankActions(&response);
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsAction("view_map", "", 1.0),
                                         IsAction("call_phone", "", 0.5)}));
}

TEST(RankingTest, DeduplicationAnnotationsByPrioritySCore) {
  ActionsSuggestionsResponse response;
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{0, 21},
                       /*text=*/"742 Evergreen Terrace"};
    annotation.entity = ClassificationResult("address", 0.5);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"view_map",
                                /*score=*/0.6,
                                /*priority_score=*/2.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{0, 21},
                       /*text=*/"742 Evergreen Terrace"};
    annotation.entity = ClassificationResult("address", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"view_map",
                                /*score=*/1.0,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{40, 53},
                       /*text=*/"1-800-TESTING"};
    annotation.entity = ClassificationResult("phone", 0.5);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"call_phone",
                                /*score=*/0.5,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }

  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()));

  ranker->RankActions(&response);
  EXPECT_THAT(
      response.actions,
      testing::ElementsAreArray(
          {IsAction("view_map", "",
                    0.6),  // lower score wins, as priority score is higher
           IsAction("call_phone", "", 0.5)}));
}

TEST(RankingTest, DeduplicatesConflictingActions) {
  ActionsSuggestionsResponse response;
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{6, 9},
                       /*text=*/"911"};
    annotation.entity = ClassificationResult("phone", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"call_phone",
                                /*score=*/1.0,
                                /*priority_score=*/1.0,
                                /*annotations=*/{annotation}});
  }
  {
    ActionSuggestionAnnotation annotation;
    annotation.span = {/*message_index=*/0, /*span=*/{4, 9},
                       /*text=*/"A-911"};
    annotation.entity = ClassificationResult("code", 1.0);
    response.actions.push_back({/*response_text=*/"",
                                /*type=*/"copy_code",
                                /*score=*/1.0,
                                /*priority_score=*/2.0,
                                /*annotations=*/{annotation}});
  }
  RankingOptionsT options;
  options.deduplicate_suggestions = true;
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RankingOptions::Pack(builder, &options));
  auto ranker = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
      flatbuffers::GetRoot<RankingOptions>(builder.GetBufferPointer()));

  ranker->RankActions(&response);
  EXPECT_THAT(response.actions,
              testing::ElementsAreArray({IsAction("copy_code", "", 1.0)}));
}

}  // namespace
}  // namespace libtextclassifier3
