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

#include <cctype>
#include <cmath>
#include <iterator>
#include <numeric>

#include "common/embedding-network.h"
#include "common/feature-extractor.h"
#include "common/memory_image/embedding-network-params-from-image.h"
#include "common/memory_image/memory-image-reader.h"
#include "common/mmap.h"
#include "common/softmax.h"
#include "smartselect/model-parser.h"
#include "smartselect/text-classification-model.pb.h"
#include "util/base/logging.h"
#include "util/utf8/unicodetext.h"
#ifndef LIBTEXTCLASSIFIER_DISABLE_ICU_SUPPORT
#include "unicode/regex.h"
#include "unicode/uchar.h"
#endif

namespace libtextclassifier {

using nlp_core::EmbeddingNetwork;
using nlp_core::EmbeddingNetworkProto;
using nlp_core::FeatureVector;
using nlp_core::MemoryImageReader;
using nlp_core::MmapFile;
using nlp_core::MmapHandle;
using nlp_core::ScopedMmap;

namespace {

int CountDigits(const std::string& str, CodepointSpan selection_indices) {
  int count = 0;
  int i = 0;
  const UnicodeText unicode_str = UTF8ToUnicodeText(str, /*do_copy=*/false);
  for (auto it = unicode_str.begin(); it != unicode_str.end(); ++it, ++i) {
    if (i >= selection_indices.first && i < selection_indices.second &&
        isdigit(*it)) {
      ++count;
    }
  }
  return count;
}

std::string ExtractSelection(const std::string& context,
                             CodepointSpan selection_indices) {
  const UnicodeText context_unicode =
      UTF8ToUnicodeText(context, /*do_copy=*/false);
  auto selection_begin = context_unicode.begin();
  std::advance(selection_begin, selection_indices.first);
  auto selection_end = context_unicode.begin();
  std::advance(selection_end, selection_indices.second);
  return UnicodeText::UTF8Substring(selection_begin, selection_end);
}

#ifndef LIBTEXTCLASSIFIER_DISABLE_ICU_SUPPORT
bool MatchesRegex(const icu::RegexPattern* regex, const std::string& context) {
  const icu::UnicodeString unicode_context(context.c_str(), context.size(),
                                           "utf-8");
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::RegexMatcher> matcher(
      regex->matcher(unicode_context, status));
  return matcher->matches(0 /* start */, status);
}
#endif

}  // namespace

TextClassificationModel::TextClassificationModel(const std::string& path)
    : mmap_(new nlp_core::ScopedMmap(path)) {
  InitFromMmap();
}

TextClassificationModel::TextClassificationModel(int fd)
    : mmap_(new nlp_core::ScopedMmap(fd)) {
  InitFromMmap();
}

TextClassificationModel::TextClassificationModel(int fd, int offset, int size)
    : mmap_(new nlp_core::ScopedMmap(fd, offset, size)) {
  InitFromMmap();
}

TextClassificationModel::TextClassificationModel(const void* addr, int size) {
  initialized_ = LoadModels(addr, size);
  if (!initialized_) {
    TC_LOG(ERROR) << "Failed to load models";
    return;
  }
}

void TextClassificationModel::InitFromMmap() {
  if (!mmap_->handle().ok()) {
    return;
  }

  initialized_ =
      LoadModels(mmap_->handle().start(), mmap_->handle().num_bytes());
  if (!initialized_) {
    TC_LOG(ERROR) << "Failed to load models";
    return;
  }
}

namespace {

// Converts sparse features vector to nlp_core::FeatureVector.
void SparseFeaturesToFeatureVector(
    const std::vector<int> sparse_features,
    const nlp_core::NumericFeatureType& feature_type,
    nlp_core::FeatureVector* result) {
  for (int feature_id : sparse_features) {
    const int64 feature_value =
        nlp_core::FloatFeatureValue(feature_id, 1.0 / sparse_features.size())
            .discrete_value;
    result->add(const_cast<nlp_core::NumericFeatureType*>(&feature_type),
                feature_value);
  }
}

// Returns a function that can be used for mapping sparse and dense features
// to a float feature vector.
// NOTE: The network object needs to be available at the time when the returned
// function object is used.
FeatureVectorFn CreateFeatureVectorFn(const EmbeddingNetwork& network,
                                      int sparse_embedding_size) {
  const nlp_core::NumericFeatureType feature_type("chargram_continuous", 0);
  return [&network, sparse_embedding_size, feature_type](
             const std::vector<int>& sparse_features,
             const std::vector<float>& dense_features, float* embedding) {
    nlp_core::FeatureVector feature_vector;
    SparseFeaturesToFeatureVector(sparse_features, feature_type,
                                  &feature_vector);

    if (network.GetEmbedding(feature_vector, 0, embedding)) {
      for (int i = 0; i < dense_features.size(); i++) {
        embedding[sparse_embedding_size + i] = dense_features[i];
      }
      return true;
    } else {
      return false;
    }
  };
}

}  // namespace

void TextClassificationModel::InitializeSharingRegexPatterns(
    const std::vector<SharingModelOptions::RegexPattern>& patterns) {
#ifndef LIBTEXTCLASSIFIER_DISABLE_ICU_SUPPORT
  // Initialize pattern recognizers.
  for (const auto& regex_pattern : patterns) {
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::RegexPattern> compiled_pattern(
        icu::RegexPattern::compile(
            icu::UnicodeString(regex_pattern.pattern().c_str(),
                               regex_pattern.pattern().size(), "utf-8"),
            0 /* flags */, status));
    if (U_FAILURE(status)) {
      TC_LOG(WARNING) << "Failed to load pattern" << regex_pattern.pattern();
    } else {
      regex_patterns_.push_back(
          {regex_pattern.collection_name(), std::move(compiled_pattern)});
    }
  }
#else
  if (!patterns.empty()) {
    TC_LOG(WARNING) << "ICU not supported regexp matchers ignored.";
  }
#endif
}

bool TextClassificationModel::LoadModels(const void* addr, int size) {
  const char *selection_model, *sharing_model;
  int selection_model_length, sharing_model_length;
  if (!ParseMergedModel(addr, size, &selection_model, &selection_model_length,
                        &sharing_model, &sharing_model_length)) {
    TC_LOG(ERROR) << "Couldn't parse the model.";
    return false;
  }

  selection_params_.reset(
      ModelParamsBuilder(selection_model, selection_model_length, nullptr));
  if (!selection_params_.get()) {
    return false;
  }
  selection_options_ = selection_params_->GetSelectionModelOptions();
  selection_network_.reset(new EmbeddingNetwork(selection_params_.get()));
  selection_feature_processor_.reset(
      new FeatureProcessor(selection_params_->GetFeatureProcessorOptions()));
  selection_feature_fn_ = CreateFeatureVectorFn(
      *selection_network_, selection_network_->EmbeddingSize(0));

  sharing_params_.reset(
      ModelParamsBuilder(sharing_model, sharing_model_length,
                         selection_params_->GetEmbeddingParams()));
  if (!sharing_params_.get()) {
    return false;
  }
  sharing_options_ = selection_params_->GetSharingModelOptions();
  sharing_network_.reset(new EmbeddingNetwork(sharing_params_.get()));
  sharing_feature_processor_.reset(
      new FeatureProcessor(sharing_params_->GetFeatureProcessorOptions()));
  sharing_feature_fn_ = CreateFeatureVectorFn(
      *sharing_network_, sharing_network_->EmbeddingSize(0));

  InitializeSharingRegexPatterns(std::vector<SharingModelOptions::RegexPattern>(
      sharing_options_.regex_pattern().begin(),
      sharing_options_.regex_pattern().end()));

  return true;
}

bool ReadSelectionModelOptions(int fd, ModelOptions* model_options) {
  ScopedMmap mmap = ScopedMmap(fd);
  if (!mmap.handle().ok()) {
    TC_LOG(ERROR) << "Can't mmap.";
    return false;
  }

  const char *selection_model, *sharing_model;
  int selection_model_length, sharing_model_length;
  if (!ParseMergedModel(mmap.handle().start(), mmap.handle().num_bytes(),
                        &selection_model, &selection_model_length,
                        &sharing_model, &sharing_model_length)) {
    TC_LOG(ERROR) << "Couldn't parse merged model.";
    return false;
  }

  MemoryImageReader<EmbeddingNetworkProto> reader(selection_model,
                                                  selection_model_length);

  auto model_options_extension_id = model_options_in_embedding_network_proto;
  if (reader.trimmed_proto().HasExtension(model_options_extension_id)) {
    *model_options =
        reader.trimmed_proto().GetExtension(model_options_extension_id);
    return true;
  } else {
    return false;
  }
}

EmbeddingNetwork::Vector TextClassificationModel::InferInternal(
    const std::string& context, CodepointSpan span,
    const FeatureProcessor& feature_processor, const EmbeddingNetwork& network,
    const FeatureVectorFn& feature_vector_fn,
    std::vector<CodepointSpan>* selection_label_spans) const {
  std::vector<Token> tokens;
  int click_pos;
  std::unique_ptr<CachedFeatures> cached_features;
  const int embedding_size = network.EmbeddingSize(0);
  if (!feature_processor.ExtractFeatures(
          context, span, /*relative_click_span=*/{0, 0},
          CreateFeatureVectorFn(network, embedding_size),
          embedding_size + feature_processor.DenseFeaturesCount(), &tokens,
          &click_pos, &cached_features)) {
    TC_VLOG(1) << "Could not extract features.";
    return {};
  }

  VectorSpan<float> features;
  VectorSpan<Token> output_tokens;
  if (!cached_features->Get(click_pos, &features, &output_tokens)) {
    TC_VLOG(1) << "Could not extract features.";
    return {};
  }

  if (selection_label_spans != nullptr) {
    if (!feature_processor.SelectionLabelSpans(output_tokens,
                                               selection_label_spans)) {
      TC_LOG(ERROR) << "Could not get spans for selection labels.";
      return {};
    }
  }

  std::vector<float> scores;
  network.ComputeLogits(features, &scores);
  return scores;
}

namespace {

// Returns true if given codepoint is contained in the given span in context.
bool IsCodepointInSpan(const char32 codepoint, const std::string& context,
                       const CodepointSpan span) {
  const UnicodeText context_unicode =
      UTF8ToUnicodeText(context, /*do_copy=*/false);

  auto begin_it = context_unicode.begin();
  std::advance(begin_it, span.first);
  auto end_it = context_unicode.begin();
  std::advance(end_it, span.second);

  return std::find(begin_it, end_it, codepoint) != end_it;
}

// Returns the first codepoint of the span.
char32 FirstSpanCodepoint(const std::string& context,
                          const CodepointSpan span) {
  const UnicodeText context_unicode =
      UTF8ToUnicodeText(context, /*do_copy=*/false);

  auto it = context_unicode.begin();
  std::advance(it, span.first);
  return *it;
}

// Returns the last codepoint of the span.
char32 LastSpanCodepoint(const std::string& context, const CodepointSpan span) {
  const UnicodeText context_unicode =
      UTF8ToUnicodeText(context, /*do_copy=*/false);

  auto it = context_unicode.begin();
  std::advance(it, span.second - 1);
  return *it;
}

}  // namespace

#ifndef LIBTEXTCLASSIFIER_DISABLE_ICU_SUPPORT

namespace {

bool IsOpenBracket(const char32 codepoint) {
  return u_getIntPropertyValue(codepoint, UCHAR_BIDI_PAIRED_BRACKET_TYPE) ==
         U_BPT_OPEN;
}

bool IsClosingBracket(const char32 codepoint) {
  return u_getIntPropertyValue(codepoint, UCHAR_BIDI_PAIRED_BRACKET_TYPE) ==
         U_BPT_CLOSE;
}

}  // namespace

// If the first or the last codepoint of the given span is a bracket, the
// bracket is stripped if the span does not contain its corresponding paired
// version.
CodepointSpan StripUnpairedBrackets(const std::string& context,
                                    CodepointSpan span) {
  if (context.empty()) {
    return span;
  }

  const char32 begin_char = FirstSpanCodepoint(context, span);

  const char32 paired_begin_char = u_getBidiPairedBracket(begin_char);
  if (paired_begin_char != begin_char) {
    if (!IsOpenBracket(begin_char) ||
        !IsCodepointInSpan(paired_begin_char, context, span)) {
      ++span.first;
    }
  }

  if (span.first == span.second) {
    return span;
  }

  const char32 end_char = LastSpanCodepoint(context, span);
  const char32 paired_end_char = u_getBidiPairedBracket(end_char);
  if (paired_end_char != end_char) {
    if (!IsClosingBracket(end_char) ||
        !IsCodepointInSpan(paired_end_char, context, span)) {
      --span.second;
    }
  }

  // Should not happen, but let's make sure.
  if (span.first > span.second) {
    TC_LOG(WARNING) << "Inverse indices result: " << span.first << ", "
                    << span.second;
    span.second = span.first;
  }

  return span;
}
#endif

CodepointSpan TextClassificationModel::SuggestSelection(
    const std::string& context, CodepointSpan click_indices) const {
  if (!initialized_) {
    TC_LOG(ERROR) << "Not initialized";
    return click_indices;
  }

  const int context_codepoint_size =
      UTF8ToUnicodeText(context, /*do_copy=*/false).size();

  if (click_indices.first < 0 || click_indices.second < 0 ||
      click_indices.first >= context_codepoint_size ||
      click_indices.second > context_codepoint_size ||
      click_indices.first >= click_indices.second) {
    TC_VLOG(1) << "Trying to run SuggestSelection with invalid indices: "
               << click_indices.first << " " << click_indices.second;
    return click_indices;
  }

  CodepointSpan result;
  if (selection_options_.enforce_symmetry()) {
    result = SuggestSelectionSymmetrical(context, click_indices);
  } else {
    float score;
    std::tie(result, score) = SuggestSelectionInternal(context, click_indices);
  }

#ifndef LIBTEXTCLASSIFIER_DISABLE_ICU_SUPPORT
  if (selection_options_.strip_unpaired_brackets()) {
    const CodepointSpan stripped_result =
        StripUnpairedBrackets(context, result);
    if (stripped_result.first != stripped_result.second) {
      result = stripped_result;
    }
  }
#endif

  return result;
}

namespace {

int BestPrediction(const std::vector<float>& scores) {
  if (!scores.empty()) {
    const int prediction =
        std::max_element(scores.begin(), scores.end()) - scores.begin();
    return prediction;
  } else {
    return kInvalidLabel;
  }
}

std::pair<CodepointSpan, float> BestSelectionSpan(
    CodepointSpan original_click_indices, const std::vector<float>& scores,
    const std::vector<CodepointSpan>& selection_label_spans) {
  const int prediction = BestPrediction(scores);
  if (prediction != kInvalidLabel) {
    std::pair<CodepointIndex, CodepointIndex> selection =
        selection_label_spans[prediction];

    if (selection.first == kInvalidIndex || selection.second == kInvalidIndex) {
      TC_VLOG(1) << "Invalid indices predicted, returning input: " << prediction
                 << " " << selection.first << " " << selection.second;
      return {original_click_indices, -1.0};
    }

    return {{selection.first, selection.second}, scores[prediction]};
  } else {
    TC_LOG(ERROR) << "Returning default selection: scores.size() = "
                  << scores.size();
    return {original_click_indices, -1.0};
  }
}

}  // namespace

std::pair<CodepointSpan, float>
TextClassificationModel::SuggestSelectionInternal(
    const std::string& context, CodepointSpan click_indices) const {
  if (!initialized_) {
    TC_LOG(ERROR) << "Not initialized";
    return {click_indices, -1.0};
  }

  std::vector<CodepointSpan> selection_label_spans;
  EmbeddingNetwork::Vector scores = InferInternal(
      context, click_indices, *selection_feature_processor_,
      *selection_network_, selection_feature_fn_, &selection_label_spans);
  scores = nlp_core::ComputeSoftmax(scores);

  return BestSelectionSpan(click_indices, scores, selection_label_spans);
}

// Implements a greedy-search-like algorithm for making selections symmetric.
//
// Steps:
// 1. Get a set of selection proposals from places around the clicked word.
// 2. For each proposal (going from highest-scoring), check if the tokens that
//    the proposal selects are still free, in which case it claims them, if a
//    proposal that contains the clicked token is found, it is returned as the
//    suggestion.
//
// This algorithm should ensure that if a selection is proposed, it does not
// matter which word of it was tapped - all of them will lead to the same
// selection.
CodepointSpan TextClassificationModel::SuggestSelectionSymmetrical(
    const std::string& context, CodepointSpan click_indices) const {
  const int symmetry_context_size = selection_options_.symmetry_context_size();
  std::vector<CodepointSpan> chunks = Chunk(
      context, click_indices, {symmetry_context_size, symmetry_context_size});
  for (const CodepointSpan& chunk : chunks) {
    // If chunk and click indices have an overlap, return the chunk.
    if (!(click_indices.first >= chunk.second ||
          click_indices.second <= chunk.first)) {
      return chunk;
    }
  }

  return click_indices;
}

std::vector<std::pair<std::string, float>>
TextClassificationModel::ClassifyText(const std::string& context,
                                      CodepointSpan selection_indices,
                                      int hint_flags) const {
  if (!initialized_) {
    TC_LOG(ERROR) << "Not initialized";
    return {};
  }

  if (std::get<0>(selection_indices) >= std::get<1>(selection_indices)) {
    TC_VLOG(1) << "Trying to run ClassifyText with invalid indices: "
               << std::get<0>(selection_indices) << " "
               << std::get<1>(selection_indices);
    return {};
  }

  if (hint_flags & SELECTION_IS_URL &&
      sharing_options_.always_accept_url_hint()) {
    return {{kUrlHintCollection, 1.0}};
  }

  if (hint_flags & SELECTION_IS_EMAIL &&
      sharing_options_.always_accept_email_hint()) {
    return {{kEmailHintCollection, 1.0}};
  }

  // Check whether any of the regular expressions match.
#ifndef LIBTEXTCLASSIFIER_DISABLE_ICU_SUPPORT
  const std::string selection_text =
      ExtractSelection(context, selection_indices);
  for (const CompiledRegexPattern& regex_pattern : regex_patterns_) {
    if (MatchesRegex(regex_pattern.pattern.get(), selection_text)) {
      return {{regex_pattern.collection_name, 1.0}};
    }
  }
#endif

  EmbeddingNetwork::Vector scores =
      InferInternal(context, selection_indices, *sharing_feature_processor_,
                    *sharing_network_, sharing_feature_fn_, nullptr);
  if (scores.empty() ||
      scores.size() != sharing_feature_processor_->NumCollections()) {
    TC_VLOG(1) << "Using default class: scores.size() = " << scores.size();
    return {};
  }

  scores = nlp_core::ComputeSoftmax(scores);

  std::vector<std::pair<std::string, float>> result(scores.size());
  for (int i = 0; i < scores.size(); i++) {
    result[i] = {sharing_feature_processor_->LabelToCollection(i), scores[i]};
  }
  std::sort(result.begin(), result.end(),
            [](const std::pair<std::string, float>& a,
               const std::pair<std::string, float>& b) {
              return a.second > b.second;
            });

  // Phone class sanity check.
  if (result.begin()->first == kPhoneCollection) {
    const int digit_count = CountDigits(context, selection_indices);
    if (digit_count < sharing_options_.phone_min_num_digits() ||
        digit_count > sharing_options_.phone_max_num_digits()) {
      return {{kOtherCollection, 1.0}};
    }
  }

  return result;
}

std::vector<CodepointSpan> TextClassificationModel::Chunk(
    const std::string& context, CodepointSpan click_span,
    TokenSpan relative_click_span) const {
  std::unique_ptr<CachedFeatures> cached_features;
  std::vector<Token> tokens;
  int click_index;
  int embedding_size = selection_network_->EmbeddingSize(0);
  if (!selection_feature_processor_->ExtractFeatures(
          context, click_span, relative_click_span, selection_feature_fn_,
          embedding_size + selection_feature_processor_->DenseFeaturesCount(),
          &tokens, &click_index, &cached_features)) {
    TC_VLOG(1) << "Couldn't ExtractFeatures.";
    return {};
  }

  int first_token;
  int last_token;
  if (relative_click_span.first == kInvalidIndex ||
      relative_click_span.second == kInvalidIndex) {
    first_token = 0;
    last_token = tokens.size();
  } else {
    first_token = click_index - relative_click_span.first;
    last_token = click_index + relative_click_span.second + 1;
  }

  struct SelectionProposal {
    int label;
    int token_index;
    CodepointSpan span;
    float score;
  };

  // Scan in the symmetry context for selection span proposals.
  std::vector<SelectionProposal> proposals;
  for (int token_index = first_token; token_index < last_token; ++token_index) {
    if (token_index < 0 || token_index >= tokens.size() ||
        tokens[token_index].is_padding) {
      continue;
    }

    float score;
    VectorSpan<float> features;
    VectorSpan<Token> output_tokens;
    std::vector<CodepointSpan> selection_label_spans;
    CodepointSpan span;
    if (cached_features->Get(token_index, &features, &output_tokens) &&
        selection_feature_processor_->SelectionLabelSpans(
            output_tokens, &selection_label_spans)) {
      // Add an implicit proposal for each token to be by itself. Every
      // token should be now represented in the results.
      proposals.push_back(
          SelectionProposal{0, token_index, selection_label_spans[0], 0.0});

      std::vector<float> scores;
      selection_network_->ComputeLogits(features, &scores);

      scores = nlp_core::ComputeSoftmax(scores);
      std::tie(span, score) = BestSelectionSpan({kInvalidIndex, kInvalidIndex},
                                                scores, selection_label_spans);
      if (span.first != kInvalidIndex && span.second != kInvalidIndex &&
          score >= 0) {
        const int prediction = BestPrediction(scores);
        proposals.push_back(
            SelectionProposal{prediction, token_index, span, score});
      }
    } else {
      // Add an implicit proposal for each token to be by itself. Every token
      // should be now represented in the results.
      proposals.push_back(SelectionProposal{
          0,
          token_index,
          {tokens[token_index].start, tokens[token_index].end},
          0.0});
    }
  }

  // Sort selection span proposals by their respective probabilities.
  std::sort(proposals.begin(), proposals.end(),
            [](const SelectionProposal& a, const SelectionProposal& b) {
              return a.score > b.score;
            });

  // Go from the highest-scoring proposal and claim tokens. Tokens are marked as
  // claimed by the higher-scoring selection proposals, so that the
  // lower-scoring ones cannot use them. Returns the selection proposal if it
  // contains the clicked token.
  std::vector<CodepointSpan> result;
  std::vector<bool> token_used(tokens.size(), false);
  for (const SelectionProposal& proposal : proposals) {
    const int predicted_label = proposal.label;
    TokenSpan relative_span;
    if (!selection_feature_processor_->LabelToTokenSpan(predicted_label,
                                                        &relative_span)) {
      continue;
    }
    TokenSpan span;
    span.first = proposal.token_index - relative_span.first;
    span.second = proposal.token_index + relative_span.second + 1;

    if (span.first != kInvalidIndex && span.second != kInvalidIndex) {
      bool feasible = true;
      for (int i = span.first; i < span.second; i++) {
        if (token_used[i]) {
          feasible = false;
          break;
        }
      }

      if (feasible) {
        result.push_back(proposal.span);
        for (int i = span.first; i < span.second; i++) {
          token_used[i] = true;
        }
      }
    }
  }

  std::sort(result.begin(), result.end(),
            [](const CodepointSpan& a, const CodepointSpan& b) {
              return a.first < b.first;
            });

  return result;
}

std::vector<TextClassificationModel::AnnotatedSpan>
TextClassificationModel::Annotate(const std::string& context) const {
  std::vector<CodepointSpan> chunks;
  const UnicodeText context_unicode = UTF8ToUnicodeText(context,
                                                        /*do_copy=*/false);
  for (const UnicodeTextRange& line :
       selection_feature_processor_->SplitContext(context_unicode)) {
    const std::vector<CodepointSpan> local_chunks =
        Chunk(UnicodeText::UTF8Substring(line.first, line.second),
              /*click_span=*/{kInvalidIndex, kInvalidIndex},
              /*relative_click_span=*/{kInvalidIndex, kInvalidIndex});
    const int offset = std::distance(context_unicode.begin(), line.first);
    for (CodepointSpan chunk : local_chunks) {
      chunks.push_back({chunk.first + offset, chunk.second + offset});
    }
  }

  std::vector<TextClassificationModel::AnnotatedSpan> result;
  for (const CodepointSpan& chunk : chunks) {
    result.emplace_back();
    result.back().span = chunk;
    result.back().classification = ClassifyText(context, chunk);
  }
  return result;
}

}  // namespace libtextclassifier
