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

#include "text-classifier.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <numeric>

#include "util/base/logging.h"
#include "util/math/softmax.h"
#include "util/utf8/unicodetext.h"

namespace libtextclassifier2 {
const std::string& TextClassifier::kOtherCollection =
    *[]() { return new std::string("other"); }();
const std::string& TextClassifier::kPhoneCollection =
    *[]() { return new std::string("phone"); }();
const std::string& TextClassifier::kDateCollection =
    *[]() { return new std::string("date"); }();

namespace {
const Model* LoadAndVerifyModel(const void* addr, int size) {
  const Model* model = GetModel(addr);

  flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(addr), size);
  if (model->Verify(verifier)) {
    return model;
  } else {
    return nullptr;
  }
}
}  // namespace

tflite::Interpreter* InterpreterManager::SelectionInterpreter() {
  if (!selection_interpreter_) {
    TC_CHECK(selection_executor_);
    selection_interpreter_ = selection_executor_->CreateInterpreter();
    if (!selection_interpreter_) {
      TC_LOG(ERROR) << "Could not build TFLite interpreter.";
    }
  }
  return selection_interpreter_.get();
}

tflite::Interpreter* InterpreterManager::ClassificationInterpreter() {
  if (!classification_interpreter_) {
    TC_CHECK(classification_executor_);
    classification_interpreter_ = classification_executor_->CreateInterpreter();
    if (!classification_interpreter_) {
      TC_LOG(ERROR) << "Could not build TFLite interpreter.";
    }
  }
  return classification_interpreter_.get();
}

std::unique_ptr<TextClassifier> TextClassifier::FromUnownedBuffer(
    const char* buffer, int size, const UniLib* unilib) {
  const Model* model = LoadAndVerifyModel(buffer, size);
  if (model == nullptr) {
    return nullptr;
  }

  auto classifier =
      std::unique_ptr<TextClassifier>(new TextClassifier(model, unilib));
  if (!classifier->IsInitialized()) {
    return nullptr;
  }

  return classifier;
}

std::unique_ptr<TextClassifier> TextClassifier::FromScopedMmap(
    std::unique_ptr<ScopedMmap>* mmap, const UniLib* unilib) {
  if (!(*mmap)->handle().ok()) {
    TC_VLOG(1) << "Mmap failed.";
    return nullptr;
  }

  const Model* model = LoadAndVerifyModel((*mmap)->handle().start(),
                                          (*mmap)->handle().num_bytes());
  if (!model) {
    TC_LOG(ERROR) << "Model verification failed.";
    return nullptr;
  }

  auto classifier =
      std::unique_ptr<TextClassifier>(new TextClassifier(mmap, model, unilib));
  if (!classifier->IsInitialized()) {
    return nullptr;
  }

  return classifier;
}

std::unique_ptr<TextClassifier> TextClassifier::FromFileDescriptor(
    int fd, int offset, int size, const UniLib* unilib) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(fd, offset, size));
  return FromScopedMmap(&mmap, unilib);
}

std::unique_ptr<TextClassifier> TextClassifier::FromFileDescriptor(
    int fd, const UniLib* unilib) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(fd));
  return FromScopedMmap(&mmap, unilib);
}

std::unique_ptr<TextClassifier> TextClassifier::FromPath(
    const std::string& path, const UniLib* unilib) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(path));
  return FromScopedMmap(&mmap, unilib);
}

void TextClassifier::ValidateAndInitialize() {
  initialized_ = false;

  if (model_ == nullptr) {
    TC_LOG(ERROR) << "No model specified.";
    return;
  }

  const bool model_enabled_for_annotation =
      (model_->triggering_options() != nullptr &&
       (model_->triggering_options()->enabled_modes() & ModeFlag_ANNOTATION));
  const bool model_enabled_for_classification =
      (model_->triggering_options() != nullptr &&
       (model_->triggering_options()->enabled_modes() &
        ModeFlag_CLASSIFICATION));
  const bool model_enabled_for_selection =
      (model_->triggering_options() != nullptr &&
       (model_->triggering_options()->enabled_modes() & ModeFlag_SELECTION));

  // Annotation requires the selection model.
  if (model_enabled_for_annotation || model_enabled_for_selection) {
    if (!model_->selection_options()) {
      TC_LOG(ERROR) << "No selection options.";
      return;
    }
    if (!model_->selection_feature_options()) {
      TC_LOG(ERROR) << "No selection feature options.";
      return;
    }
    if (!model_->selection_feature_options()->bounds_sensitive_features()) {
      TC_LOG(ERROR) << "No selection bounds sensitive feature options.";
      return;
    }
    if (!model_->selection_model()) {
      TC_LOG(ERROR) << "No selection model.";
      return;
    }
    selection_executor_.reset(
        new ModelExecutor(flatbuffers::GetRoot<tflite::Model>(
            model_->selection_model()->data())));
    if (!selection_executor_) {
      TC_LOG(ERROR) << "Could not initialize selection executor.";
      return;
    }
    selection_feature_processor_.reset(
        new FeatureProcessor(model_->selection_feature_options(), unilib_));
  }

  // Annotation requires the classification model for conflict resolution and
  // scoring.
  // Selection requires the classification model for conflict resolution.
  if (model_enabled_for_annotation || model_enabled_for_classification ||
      model_enabled_for_selection) {
    if (!model_->classification_options()) {
      TC_LOG(ERROR) << "No classification options.";
      return;
    }

    if (!model_->classification_feature_options()) {
      TC_LOG(ERROR) << "No classification feature options.";
      return;
    }

    if (!model_->classification_feature_options()
             ->bounds_sensitive_features()) {
      TC_LOG(ERROR) << "No classification bounds sensitive feature options.";
      return;
    }
    if (!model_->classification_model()) {
      TC_LOG(ERROR) << "No clf model.";
      return;
    }

    classification_executor_.reset(
        new ModelExecutor(flatbuffers::GetRoot<tflite::Model>(
            model_->classification_model()->data())));
    if (!classification_executor_) {
      TC_LOG(ERROR) << "Could not initialize classification executor.";
      return;
    }

    classification_feature_processor_.reset(new FeatureProcessor(
        model_->classification_feature_options(), unilib_));
  }

  // The embeddings need to be specified if the model is to be used for
  // classification or selection.
  if (model_enabled_for_annotation || model_enabled_for_classification ||
      model_enabled_for_selection) {
    if (!model_->embedding_model()) {
      TC_LOG(ERROR) << "No embedding model.";
      return;
    }

    // Check that the embedding size of the selection and classification model
    // matches, as they are using the same embeddings.
    if (model_enabled_for_selection &&
        (model_->selection_feature_options()->embedding_size() !=
             model_->classification_feature_options()->embedding_size() ||
         model_->selection_feature_options()->embedding_quantization_bits() !=
             model_->classification_feature_options()
                 ->embedding_quantization_bits())) {
      TC_LOG(ERROR) << "Mismatching embedding size/quantization.";
      return;
    }

    embedding_executor_.reset(new TFLiteEmbeddingExecutor(
        flatbuffers::GetRoot<tflite::Model>(model_->embedding_model()->data()),
        model_->classification_feature_options()->embedding_size(),
        model_->classification_feature_options()
            ->embedding_quantization_bits()));
    if (!embedding_executor_ || !embedding_executor_->IsReady()) {
      TC_LOG(ERROR) << "Could not initialize embedding executor.";
      return;
    }
  }

  if (model_->regex_model()) {
    if (!InitializeRegexModel()) {
      TC_LOG(ERROR) << "Could not initialize regex model.";
    }
  }

  if (model_->datetime_model()) {
    datetime_parser_ =
        DatetimeParser::Instance(model_->datetime_model(), *unilib_);
    if (!datetime_parser_) {
      TC_LOG(ERROR) << "Could not initialize datetime parser.";
      return;
    }
  }

  initialized_ = true;
}

bool TextClassifier::InitializeRegexModel() {
  if (!model_->regex_model()->patterns()) {
    initialized_ = false;
    TC_LOG(ERROR) << "No patterns in the regex config.";
    return false;
  }

  // Initialize pattern recognizers.
  int regex_pattern_id = 0;
  for (const auto& regex_pattern : *model_->regex_model()->patterns()) {
    std::unique_ptr<UniLib::RegexPattern> compiled_pattern(
        unilib_->CreateRegexPattern(UTF8ToUnicodeText(
            regex_pattern->pattern()->c_str(),
            regex_pattern->pattern()->Length(), /*do_copy=*/false)));

    if (!compiled_pattern) {
      TC_LOG(INFO) << "Failed to load pattern"
                   << regex_pattern->pattern()->str();
      continue;
    }

    if (regex_pattern->enabled_modes() & ModeFlag_ANNOTATION) {
      annotation_regex_patterns_.push_back(regex_pattern_id);
    }
    if (regex_pattern->enabled_modes() & ModeFlag_CLASSIFICATION) {
      classification_regex_patterns_.push_back(regex_pattern_id);
    }
    if (regex_pattern->enabled_modes() & ModeFlag_SELECTION) {
      selection_regex_patterns_.push_back(regex_pattern_id);
    }
    regex_patterns_.push_back({regex_pattern->collection_name()->str(),
                               regex_pattern->target_classification_score(),
                               regex_pattern->priority_score(),
                               std::move(compiled_pattern)});
    if (regex_pattern->use_approximate_matching()) {
      regex_approximate_match_pattern_ids_.insert(regex_pattern_id);
    }
    ++regex_pattern_id;
  }

  return true;
}

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
}  // namespace

CodepointSpan TextClassifier::SuggestSelection(
    const std::string& context, CodepointSpan click_indices,
    const SelectionOptions& options) const {
  if (!initialized_) {
    TC_LOG(ERROR) << "Not initialized";
    return click_indices;
  }
  if (!(model_->enabled_modes() & ModeFlag_SELECTION)) {
    return click_indices;
  }

  const UnicodeText context_unicode = UTF8ToUnicodeText(context,
                                                        /*do_copy=*/false);
  const int context_codepoint_size = context_unicode.size_codepoints();

  if (click_indices.first < 0 || click_indices.second < 0 ||
      click_indices.first >= context_codepoint_size ||
      click_indices.second > context_codepoint_size ||
      click_indices.first >= click_indices.second) {
    TC_VLOG(1) << "Trying to run SuggestSelection with invalid indices: "
               << click_indices.first << " " << click_indices.second;
    return click_indices;
  }

  std::vector<AnnotatedSpan> candidates;
  InterpreterManager interpreter_manager(selection_executor_.get(),
                                         classification_executor_.get());
  if (!ModelSuggestSelection(context_unicode, click_indices,
                             &interpreter_manager, &candidates)) {
    TC_LOG(ERROR) << "Model suggest selection failed.";
    return click_indices;
  }
  if (!RegexChunk(context_unicode, selection_regex_patterns_, &candidates)) {
    TC_LOG(ERROR) << "Regex suggest selection failed.";
    return click_indices;
  }
  if (!DatetimeChunk(UTF8ToUnicodeText(context, /*do_copy=*/false),
                     /*reference_time_ms_utc=*/0, /*reference_timezone=*/"",
                     options.locales, ModeFlag_SELECTION, &candidates)) {
    TC_LOG(ERROR) << "Datetime suggest selection failed.";
    return click_indices;
  }

  // Sort candidates according to their position in the input, so that the next
  // code can assume that any connected component of overlapping spans forms a
  // contiguous block.
  std::sort(candidates.begin(), candidates.end(),
            [](const AnnotatedSpan& a, const AnnotatedSpan& b) {
              return a.span.first < b.span.first;
            });

  std::vector<int> candidate_indices;
  if (!ResolveConflicts(candidates, context, &interpreter_manager,
                        &candidate_indices)) {
    TC_LOG(ERROR) << "Couldn't resolve conflicts.";
    return click_indices;
  }

  for (const int i : candidate_indices) {
    if (SpansOverlap(candidates[i].span, click_indices)) {
      return candidates[i].span;
    }
  }

  return click_indices;
}

namespace {
// Helper function that returns the index of the first candidate that
// transitively does not overlap with the candidate on 'start_index'. If the end
// of 'candidates' is reached, it returns the index that points right behind the
// array.
int FirstNonOverlappingSpanIndex(const std::vector<AnnotatedSpan>& candidates,
                                 int start_index) {
  int first_non_overlapping = start_index + 1;
  CodepointSpan conflicting_span = candidates[start_index].span;
  while (
      first_non_overlapping < candidates.size() &&
      SpansOverlap(conflicting_span, candidates[first_non_overlapping].span)) {
    // Grow the span to include the current one.
    conflicting_span.second = std::max(
        conflicting_span.second, candidates[first_non_overlapping].span.second);

    ++first_non_overlapping;
  }
  return first_non_overlapping;
}
}  // namespace

bool TextClassifier::ResolveConflicts(
    const std::vector<AnnotatedSpan>& candidates, const std::string& context,
    InterpreterManager* interpreter_manager, std::vector<int>* result) const {
  result->clear();
  result->reserve(candidates.size());
  for (int i = 0; i < candidates.size();) {
    int first_non_overlapping =
        FirstNonOverlappingSpanIndex(candidates, /*start_index=*/i);

    const bool conflict_found = first_non_overlapping != (i + 1);
    if (conflict_found) {
      std::vector<int> candidate_indices;
      if (!ResolveConflict(context, candidates, i, first_non_overlapping,
                           interpreter_manager, &candidate_indices)) {
        return false;
      }
      result->insert(result->end(), candidate_indices.begin(),
                     candidate_indices.end());
    } else {
      result->push_back(i);
    }

    // Skip over the whole conflicting group/go to next candidate.
    i = first_non_overlapping;
  }
  return true;
}

namespace {
inline bool ClassifiedAsOther(
    const std::vector<ClassificationResult>& classification) {
  return !classification.empty() &&
         classification[0].collection == TextClassifier::kOtherCollection;
}

float GetPriorityScore(
    const std::vector<ClassificationResult>& classification) {
  if (!ClassifiedAsOther(classification)) {
    return classification[0].priority_score;
  } else {
    return -1.0;
  }
}
}  // namespace

bool TextClassifier::ResolveConflict(
    const std::string& context, const std::vector<AnnotatedSpan>& candidates,
    int start_index, int end_index, InterpreterManager* interpreter_manager,
    std::vector<int>* chosen_indices) const {
  std::vector<int> conflicting_indices;
  std::unordered_map<int, float> scores;
  for (int i = start_index; i < end_index; ++i) {
    conflicting_indices.push_back(i);
    if (!candidates[i].classification.empty()) {
      scores[i] = GetPriorityScore(candidates[i].classification);
      continue;
    }

    // OPTIMIZATION: So that we don't have to classify all the ML model
    // spans apriori, we wait until we get here, when they conflict with
    // something and we need the actual classification scores. So if the
    // candidate conflicts and comes from the model, we need to run a
    // classification to determine its priority:
    std::vector<ClassificationResult> classification;
    if (!ModelClassifyText(context, candidates[i].span, interpreter_manager,
                           /*embedding_cache=*/nullptr, &classification)) {
      return false;
    }

    if (!classification.empty()) {
      scores[i] = GetPriorityScore(classification);
    }
  }

  std::sort(conflicting_indices.begin(), conflicting_indices.end(),
            [&scores](int i, int j) { return scores[i] > scores[j]; });

  // Keeps the candidates sorted by their position in the text (their left span
  // index) for fast retrieval down.
  std::set<int, std::function<bool(int, int)>> chosen_indices_set(
      [&candidates](int a, int b) {
        return candidates[a].span.first < candidates[b].span.first;
      });

  // Greedily place the candidates if they don't conflict with the already
  // placed ones.
  for (int i = 0; i < conflicting_indices.size(); ++i) {
    const int considered_candidate = conflicting_indices[i];
    if (!DoesCandidateConflict(considered_candidate, candidates,
                               chosen_indices_set)) {
      chosen_indices_set.insert(considered_candidate);
    }
  }

  *chosen_indices =
      std::vector<int>(chosen_indices_set.begin(), chosen_indices_set.end());

  return true;
}

bool TextClassifier::ModelSuggestSelection(
    const UnicodeText& context_unicode, CodepointSpan click_indices,
    InterpreterManager* interpreter_manager,
    std::vector<AnnotatedSpan>* result) const {
  if (model_->triggering_options() == nullptr ||
      !(model_->triggering_options()->enabled_modes() & ModeFlag_SELECTION)) {
    return true;
  }

  int click_pos;
  std::vector<Token> tokens =
      selection_feature_processor_->Tokenize(context_unicode);
  selection_feature_processor_->RetokenizeAndFindClick(
      context_unicode, click_indices,
      selection_feature_processor_->GetOptions()->only_use_line_with_click(),
      &tokens, &click_pos);
  if (click_pos == kInvalidIndex) {
    TC_VLOG(1) << "Could not calculate the click position.";
    return false;
  }

  const int symmetry_context_size =
      model_->selection_options()->symmetry_context_size();
  const FeatureProcessorOptions_::BoundsSensitiveFeatures*
      bounds_sensitive_features = selection_feature_processor_->GetOptions()
                                      ->bounds_sensitive_features();

  // The symmetry context span is the clicked token with symmetry_context_size
  // tokens on either side.
  const TokenSpan symmetry_context_span = IntersectTokenSpans(
      ExpandTokenSpan(SingleTokenSpan(click_pos),
                      /*num_tokens_left=*/symmetry_context_size,
                      /*num_tokens_right=*/symmetry_context_size),
      {0, tokens.size()});

  // Compute the extraction span based on the model type.
  TokenSpan extraction_span;
  if (bounds_sensitive_features && bounds_sensitive_features->enabled()) {
    // The extraction span is the symmetry context span expanded to include
    // max_selection_span tokens on either side, which is how far a selection
    // can stretch from the click, plus a relevant number of tokens outside of
    // the bounds of the selection.
    const int max_selection_span =
        selection_feature_processor_->GetOptions()->max_selection_span();
    extraction_span =
        ExpandTokenSpan(symmetry_context_span,
                        /*num_tokens_left=*/max_selection_span +
                            bounds_sensitive_features->num_tokens_before(),
                        /*num_tokens_right=*/max_selection_span +
                            bounds_sensitive_features->num_tokens_after());
  } else {
    // The extraction span is the symmetry context span expanded to include
    // context_size tokens on either side.
    const int context_size =
        selection_feature_processor_->GetOptions()->context_size();
    extraction_span = ExpandTokenSpan(symmetry_context_span,
                                      /*num_tokens_left=*/context_size,
                                      /*num_tokens_right=*/context_size);
  }
  extraction_span = IntersectTokenSpans(extraction_span, {0, tokens.size()});

  std::unique_ptr<CachedFeatures> cached_features;
  if (!selection_feature_processor_->ExtractFeatures(
          tokens, extraction_span,
          /*selection_span_for_feature=*/{kInvalidIndex, kInvalidIndex},
          embedding_executor_.get(),
          /*embedding_cache=*/nullptr,
          selection_feature_processor_->EmbeddingSize() +
              selection_feature_processor_->DenseFeaturesCount(),
          &cached_features)) {
    TC_LOG(ERROR) << "Could not extract features.";
    return false;
  }

  // Produce selection model candidates.
  std::vector<TokenSpan> chunks;
  if (!ModelChunk(tokens.size(), /*span_of_interest=*/symmetry_context_span,
                  interpreter_manager->SelectionInterpreter(), *cached_features,
                  &chunks)) {
    TC_LOG(ERROR) << "Could not chunk.";
    return false;
  }

  for (const TokenSpan& chunk : chunks) {
    AnnotatedSpan candidate;
    candidate.span = selection_feature_processor_->StripBoundaryCodepoints(
        context_unicode, TokenSpanToCodepointSpan(tokens, chunk));
    if (model_->selection_options()->strip_unpaired_brackets()) {
      candidate.span =
          StripUnpairedBrackets(context_unicode, candidate.span, *unilib_);
    }

    // Only output non-empty spans.
    if (candidate.span.first != candidate.span.second) {
      result->push_back(candidate);
    }
  }
  return true;
}

bool TextClassifier::ModelClassifyText(
    const std::string& context, CodepointSpan selection_indices,
    InterpreterManager* interpreter_manager,
    FeatureProcessor::EmbeddingCache* embedding_cache,
    std::vector<ClassificationResult>* classification_results) const {
  if (model_->triggering_options() == nullptr ||
      !(model_->triggering_options()->enabled_modes() &
        ModeFlag_CLASSIFICATION)) {
    return true;
  }
  return ModelClassifyText(context, {}, selection_indices, interpreter_manager,
                           embedding_cache, classification_results);
}

namespace internal {
std::vector<Token> CopyCachedTokens(const std::vector<Token>& cached_tokens,
                                    CodepointSpan selection_indices,
                                    TokenSpan tokens_around_selection_to_copy) {
  const auto first_selection_token = std::upper_bound(
      cached_tokens.begin(), cached_tokens.end(), selection_indices.first,
      [](int selection_start, const Token& token) {
        return selection_start < token.end;
      });
  const auto last_selection_token = std::lower_bound(
      cached_tokens.begin(), cached_tokens.end(), selection_indices.second,
      [](const Token& token, int selection_end) {
        return token.start < selection_end;
      });

  const int64 first_token = std::max(
      static_cast<int64>(0),
      static_cast<int64>((first_selection_token - cached_tokens.begin()) -
                         tokens_around_selection_to_copy.first));
  const int64 last_token = std::min(
      static_cast<int64>(cached_tokens.size()),
      static_cast<int64>((last_selection_token - cached_tokens.begin()) +
                         tokens_around_selection_to_copy.second));

  std::vector<Token> tokens;
  tokens.reserve(last_token - first_token);
  for (int i = first_token; i < last_token; ++i) {
    tokens.push_back(cached_tokens[i]);
  }
  return tokens;
}
}  // namespace internal

TokenSpan TextClassifier::ClassifyTextUpperBoundNeededTokens() const {
  const FeatureProcessorOptions_::BoundsSensitiveFeatures*
      bounds_sensitive_features =
          classification_feature_processor_->GetOptions()
              ->bounds_sensitive_features();
  if (bounds_sensitive_features && bounds_sensitive_features->enabled()) {
    // The extraction span is the selection span expanded to include a relevant
    // number of tokens outside of the bounds of the selection.
    return {bounds_sensitive_features->num_tokens_before(),
            bounds_sensitive_features->num_tokens_after()};
  } else {
    // The extraction span is the clicked token with context_size tokens on
    // either side.
    const int context_size =
        selection_feature_processor_->GetOptions()->context_size();
    return {context_size, context_size};
  }
}

bool TextClassifier::ModelClassifyText(
    const std::string& context, const std::vector<Token>& cached_tokens,
    CodepointSpan selection_indices, InterpreterManager* interpreter_manager,
    FeatureProcessor::EmbeddingCache* embedding_cache,
    std::vector<ClassificationResult>* classification_results) const {
  std::vector<Token> tokens;
  if (cached_tokens.empty()) {
    tokens = classification_feature_processor_->Tokenize(context);
  } else {
    tokens = internal::CopyCachedTokens(cached_tokens, selection_indices,
                                        ClassifyTextUpperBoundNeededTokens());
  }

  int click_pos;
  classification_feature_processor_->RetokenizeAndFindClick(
      context, selection_indices,
      classification_feature_processor_->GetOptions()
          ->only_use_line_with_click(),
      &tokens, &click_pos);
  const TokenSpan selection_token_span =
      CodepointSpanToTokenSpan(tokens, selection_indices);
  const FeatureProcessorOptions_::BoundsSensitiveFeatures*
      bounds_sensitive_features =
          classification_feature_processor_->GetOptions()
              ->bounds_sensitive_features();
  if (selection_token_span.first == kInvalidIndex ||
      selection_token_span.second == kInvalidIndex) {
    TC_LOG(ERROR) << "Could not determine span.";
    return false;
  }

  // Compute the extraction span based on the model type.
  TokenSpan extraction_span;
  if (bounds_sensitive_features && bounds_sensitive_features->enabled()) {
    // The extraction span is the selection span expanded to include a relevant
    // number of tokens outside of the bounds of the selection.
    extraction_span = ExpandTokenSpan(
        selection_token_span,
        /*num_tokens_left=*/bounds_sensitive_features->num_tokens_before(),
        /*num_tokens_right=*/bounds_sensitive_features->num_tokens_after());
  } else {
    if (click_pos == kInvalidIndex) {
      TC_LOG(ERROR) << "Couldn't choose a click position.";
      return false;
    }
    // The extraction span is the clicked token with context_size tokens on
    // either side.
    const int context_size =
        classification_feature_processor_->GetOptions()->context_size();
    extraction_span = ExpandTokenSpan(SingleTokenSpan(click_pos),
                                      /*num_tokens_left=*/context_size,
                                      /*num_tokens_right=*/context_size);
  }
  extraction_span = IntersectTokenSpans(extraction_span, {0, tokens.size()});

  std::unique_ptr<CachedFeatures> cached_features;
  if (!classification_feature_processor_->ExtractFeatures(
          tokens, extraction_span, selection_indices, embedding_executor_.get(),
          embedding_cache,
          classification_feature_processor_->EmbeddingSize() +
              classification_feature_processor_->DenseFeaturesCount(),
          &cached_features)) {
    TC_LOG(ERROR) << "Could not extract features.";
    return false;
  }

  std::vector<float> features;
  features.reserve(cached_features->OutputFeaturesSize());
  if (bounds_sensitive_features && bounds_sensitive_features->enabled()) {
    cached_features->AppendBoundsSensitiveFeaturesForSpan(selection_token_span,
                                                          &features);
  } else {
    cached_features->AppendClickContextFeaturesForClick(click_pos, &features);
  }

  TensorView<float> logits = classification_executor_->ComputeLogits(
      TensorView<float>(features.data(),
                        {1, static_cast<int>(features.size())}),
      interpreter_manager->ClassificationInterpreter());
  if (!logits.is_valid()) {
    TC_LOG(ERROR) << "Couldn't compute logits.";
    return false;
  }

  if (logits.dims() != 2 || logits.dim(0) != 1 ||
      logits.dim(1) != classification_feature_processor_->NumCollections()) {
    TC_LOG(ERROR) << "Mismatching output";
    return false;
  }

  const std::vector<float> scores =
      ComputeSoftmax(logits.data(), logits.dim(1));

  classification_results->resize(scores.size());
  for (int i = 0; i < scores.size(); i++) {
    (*classification_results)[i] = {
        classification_feature_processor_->LabelToCollection(i), scores[i]};
  }
  std::sort(classification_results->begin(), classification_results->end(),
            [](const ClassificationResult& a, const ClassificationResult& b) {
              return a.score > b.score;
            });

  // Phone class sanity check.
  if (!classification_results->empty() &&
      classification_results->begin()->collection == kPhoneCollection) {
    const int digit_count = CountDigits(context, selection_indices);
    if (digit_count <
            model_->classification_options()->phone_min_num_digits() ||
        digit_count >
            model_->classification_options()->phone_max_num_digits()) {
      *classification_results = {{kOtherCollection, 1.0}};
    }
  }

  return true;
}

bool TextClassifier::RegexClassifyText(
    const std::string& context, CodepointSpan selection_indices,
    ClassificationResult* classification_result) const {
  const std::string selection_text =
      ExtractSelection(context, selection_indices);
  const UnicodeText selection_text_unicode(
      UTF8ToUnicodeText(selection_text, /*do_copy=*/false));

  // Check whether any of the regular expressions match.
  for (const int pattern_id : classification_regex_patterns_) {
    const CompiledRegexPattern& regex_pattern = regex_patterns_[pattern_id];
    const std::unique_ptr<UniLib::RegexMatcher> matcher =
        regex_pattern.pattern->Matcher(selection_text_unicode);
    int status = UniLib::RegexMatcher::kNoError;
    bool matches;
    if (regex_approximate_match_pattern_ids_.find(pattern_id) !=
        regex_approximate_match_pattern_ids_.end()) {
      matches = matcher->ApproximatelyMatches(&status);
    } else {
      matches = matcher->Matches(&status);
    }
    if (status != UniLib::RegexMatcher::kNoError) {
      return false;
    }
    if (matches) {
      *classification_result = {regex_pattern.collection_name,
                                regex_pattern.target_classification_score,
                                regex_pattern.priority_score};
      return true;
    }
    if (status != UniLib::RegexMatcher::kNoError) {
      TC_LOG(ERROR) << "Cound't match regex: " << pattern_id;
    }
  }

  return false;
}

bool TextClassifier::DatetimeClassifyText(
    const std::string& context, CodepointSpan selection_indices,
    const ClassificationOptions& options,
    ClassificationResult* classification_result) const {
  if (!datetime_parser_) {
    return false;
  }

  const std::string selection_text =
      ExtractSelection(context, selection_indices);

  std::vector<DatetimeParseResultSpan> datetime_spans;
  if (!datetime_parser_->Parse(selection_text, options.reference_time_ms_utc,
                               options.reference_timezone, options.locales,
                               ModeFlag_CLASSIFICATION, &datetime_spans)) {
    TC_LOG(ERROR) << "Error during parsing datetime.";
    return false;
  }
  for (const DatetimeParseResultSpan& datetime_span : datetime_spans) {
    // Only consider the result valid if the selection and extracted datetime
    // spans exactly match.
    if (std::make_pair(datetime_span.span.first + selection_indices.first,
                       datetime_span.span.second + selection_indices.first) ==
        selection_indices) {
      *classification_result = {kDateCollection,
                                datetime_span.target_classification_score};
      classification_result->datetime_parse_result = datetime_span.data;
      return true;
    }
  }
  return false;
}

std::vector<ClassificationResult> TextClassifier::ClassifyText(
    const std::string& context, CodepointSpan selection_indices,
    const ClassificationOptions& options) const {
  if (!initialized_) {
    TC_LOG(ERROR) << "Not initialized";
    return {};
  }

  if (!(model_->enabled_modes() & ModeFlag_CLASSIFICATION)) {
    return {};
  }

  if (std::get<0>(selection_indices) >= std::get<1>(selection_indices)) {
    TC_VLOG(1) << "Trying to run ClassifyText with invalid indices: "
               << std::get<0>(selection_indices) << " "
               << std::get<1>(selection_indices);
    return {};
  }

  // Try the regular expression models.
  ClassificationResult regex_result;
  if (RegexClassifyText(context, selection_indices, &regex_result)) {
    return {regex_result};
  }

  // Try the date model.
  ClassificationResult datetime_result;
  if (DatetimeClassifyText(context, selection_indices, options,
                           &datetime_result)) {
    return {datetime_result};
  }

  // Fallback to the model.
  std::vector<ClassificationResult> model_result;

  InterpreterManager interpreter_manager(selection_executor_.get(),
                                         classification_executor_.get());
  if (ModelClassifyText(context, selection_indices, &interpreter_manager,
                        /*embedding_cache=*/nullptr, &model_result)) {
    return model_result;
  }

  // No classifications.
  return {};
}

bool TextClassifier::ModelAnnotate(const std::string& context,
                                   InterpreterManager* interpreter_manager,
                                   std::vector<AnnotatedSpan>* result) const {
  if (model_->triggering_options() == nullptr ||
      !(model_->triggering_options()->enabled_modes() & ModeFlag_ANNOTATION)) {
    return true;
  }

  const UnicodeText context_unicode = UTF8ToUnicodeText(context,
                                                        /*do_copy=*/false);
  std::vector<UnicodeTextRange> lines;
  if (!selection_feature_processor_->GetOptions()->only_use_line_with_click()) {
    lines.push_back({context_unicode.begin(), context_unicode.end()});
  } else {
    lines = selection_feature_processor_->SplitContext(context_unicode);
  }

  const float min_annotate_confidence =
      (model_->triggering_options() != nullptr
           ? model_->triggering_options()->min_annotate_confidence()
           : 0.f);

  FeatureProcessor::EmbeddingCache embedding_cache;
  for (const UnicodeTextRange& line : lines) {
    const std::string line_str =
        UnicodeText::UTF8Substring(line.first, line.second);

    std::vector<Token> tokens =
        selection_feature_processor_->Tokenize(line_str);
    selection_feature_processor_->RetokenizeAndFindClick(
        line_str, {0, std::distance(line.first, line.second)},
        selection_feature_processor_->GetOptions()->only_use_line_with_click(),
        &tokens,
        /*click_pos=*/nullptr);
    const TokenSpan full_line_span = {0, tokens.size()};

    std::unique_ptr<CachedFeatures> cached_features;
    if (!selection_feature_processor_->ExtractFeatures(
            tokens, full_line_span,
            /*selection_span_for_feature=*/{kInvalidIndex, kInvalidIndex},
            embedding_executor_.get(),
            /*embedding_cache=*/nullptr,
            selection_feature_processor_->EmbeddingSize() +
                selection_feature_processor_->DenseFeaturesCount(),
            &cached_features)) {
      TC_LOG(ERROR) << "Could not extract features.";
      return false;
    }

    std::vector<TokenSpan> local_chunks;
    if (!ModelChunk(tokens.size(), /*span_of_interest=*/full_line_span,
                    interpreter_manager->SelectionInterpreter(),
                    *cached_features, &local_chunks)) {
      TC_LOG(ERROR) << "Could not chunk.";
      return false;
    }

    const int offset = std::distance(context_unicode.begin(), line.first);
    for (const TokenSpan& chunk : local_chunks) {
      const CodepointSpan codepoint_span =
          selection_feature_processor_->StripBoundaryCodepoints(
              line_str, TokenSpanToCodepointSpan(tokens, chunk));

      // Skip empty spans.
      if (codepoint_span.first != codepoint_span.second) {
        std::vector<ClassificationResult> classification;
        if (!ModelClassifyText(line_str, tokens, codepoint_span,
                               interpreter_manager, &embedding_cache,
                               &classification)) {
          TC_LOG(ERROR) << "Could not classify text: "
                        << (codepoint_span.first + offset) << " "
                        << (codepoint_span.second + offset);
          return false;
        }

        // Do not include the span if it's classified as "other".
        if (!classification.empty() && !ClassifiedAsOther(classification) &&
            classification[0].score >= min_annotate_confidence) {
          AnnotatedSpan result_span;
          result_span.span = {codepoint_span.first + offset,
                              codepoint_span.second + offset};
          result_span.classification = std::move(classification);
          result->push_back(std::move(result_span));
        }
      }
    }
  }
  return true;
}

const FeatureProcessor& TextClassifier::SelectionFeatureProcessorForTests()
    const {
  return *selection_feature_processor_;
}

std::vector<AnnotatedSpan> TextClassifier::Annotate(
    const std::string& context, const AnnotationOptions& options) const {
  std::vector<AnnotatedSpan> candidates;

  if (!(model_->enabled_modes() & ModeFlag_ANNOTATION)) {
    return {};
  }

  InterpreterManager interpreter_manager(selection_executor_.get(),
                                         classification_executor_.get());
  // Annotate with the selection model.
  if (!ModelAnnotate(context, &interpreter_manager, &candidates)) {
    TC_LOG(ERROR) << "Couldn't run ModelAnnotate.";
    return {};
  }

  // Annotate with the regular expression models.
  if (!RegexChunk(UTF8ToUnicodeText(context, /*do_copy=*/false),
                  annotation_regex_patterns_, &candidates)) {
    TC_LOG(ERROR) << "Couldn't run RegexChunk.";
    return {};
  }

  // Annotate with the datetime model.
  if (!DatetimeChunk(UTF8ToUnicodeText(context, /*do_copy=*/false),
                     options.reference_time_ms_utc, options.reference_timezone,
                     options.locales, ModeFlag_ANNOTATION, &candidates)) {
    TC_LOG(ERROR) << "Couldn't run RegexChunk.";
    return {};
  }

  // Sort candidates according to their position in the input, so that the next
  // code can assume that any connected component of overlapping spans forms a
  // contiguous block.
  std::sort(candidates.begin(), candidates.end(),
            [](const AnnotatedSpan& a, const AnnotatedSpan& b) {
              return a.span.first < b.span.first;
            });

  std::vector<int> candidate_indices;
  if (!ResolveConflicts(candidates, context, &interpreter_manager,
                        &candidate_indices)) {
    TC_LOG(ERROR) << "Couldn't resolve conflicts.";
    return {};
  }

  std::vector<AnnotatedSpan> result;
  result.reserve(candidate_indices.size());
  for (const int i : candidate_indices) {
    if (!candidates[i].classification.empty() &&
        !ClassifiedAsOther(candidates[i].classification)) {
      result.push_back(std::move(candidates[i]));
    }
  }

  return result;
}

bool TextClassifier::RegexChunk(const UnicodeText& context_unicode,
                                const std::vector<int>& rules,
                                std::vector<AnnotatedSpan>* result) const {
  for (int pattern_id : rules) {
    const CompiledRegexPattern& regex_pattern = regex_patterns_[pattern_id];
    const auto matcher = regex_pattern.pattern->Matcher(context_unicode);
    if (!matcher) {
      TC_LOG(ERROR) << "Could not get regex matcher for pattern: "
                    << pattern_id;
      return false;
    }

    int status = UniLib::RegexMatcher::kNoError;
    while (matcher->Find(&status) && status == UniLib::RegexMatcher::kNoError) {
      result->emplace_back();
      // Selection/annotation regular expressions need to specify a capturing
      // group specifying the selection.
      result->back().span = {matcher->Start(1, &status),
                             matcher->End(1, &status)};
      result->back().classification = {
          {regex_pattern.collection_name,
           regex_pattern.target_classification_score,
           regex_pattern.priority_score}};
    }
  }
  return true;
}

bool TextClassifier::ModelChunk(int num_tokens,
                                const TokenSpan& span_of_interest,
                                tflite::Interpreter* selection_interpreter,
                                const CachedFeatures& cached_features,
                                std::vector<TokenSpan>* chunks) const {
  const int max_selection_span =
      selection_feature_processor_->GetOptions()->max_selection_span();
  // The inference span is the span of interest expanded to include
  // max_selection_span tokens on either side, which is how far a selection can
  // stretch from the click.
  const TokenSpan inference_span = IntersectTokenSpans(
      ExpandTokenSpan(span_of_interest,
                      /*num_tokens_left=*/max_selection_span,
                      /*num_tokens_right=*/max_selection_span),
      {0, num_tokens});

  std::vector<ScoredChunk> scored_chunks;
  if (selection_feature_processor_->GetOptions()->bounds_sensitive_features() &&
      selection_feature_processor_->GetOptions()
          ->bounds_sensitive_features()
          ->enabled()) {
    if (!ModelBoundsSensitiveScoreChunks(
            num_tokens, span_of_interest, inference_span, cached_features,
            selection_interpreter, &scored_chunks)) {
      return false;
    }
  } else {
    if (!ModelClickContextScoreChunks(num_tokens, span_of_interest,
                                      cached_features, selection_interpreter,
                                      &scored_chunks)) {
      return false;
    }
  }
  std::sort(scored_chunks.rbegin(), scored_chunks.rend(),
            [](const ScoredChunk& lhs, const ScoredChunk& rhs) {
              return lhs.score < rhs.score;
            });

  // Traverse the candidate chunks from highest-scoring to lowest-scoring. Pick
  // them greedily as long as they do not overlap with any previously picked
  // chunks.
  std::vector<bool> token_used(TokenSpanSize(inference_span));
  chunks->clear();
  for (const ScoredChunk& scored_chunk : scored_chunks) {
    bool feasible = true;
    for (int i = scored_chunk.token_span.first;
         i < scored_chunk.token_span.second; ++i) {
      if (token_used[i - inference_span.first]) {
        feasible = false;
        break;
      }
    }

    if (!feasible) {
      continue;
    }

    for (int i = scored_chunk.token_span.first;
         i < scored_chunk.token_span.second; ++i) {
      token_used[i - inference_span.first] = true;
    }

    chunks->push_back(scored_chunk.token_span);
  }

  std::sort(chunks->begin(), chunks->end());

  return true;
}

namespace {
// Updates the value at the given key in the map to maximum of the current value
// and the given value, or simply inserts the value if the key is not yet there.
template <typename Map>
void UpdateMax(Map* map, typename Map::key_type key,
               typename Map::mapped_type value) {
  const auto it = map->find(key);
  if (it != map->end()) {
    it->second = std::max(it->second, value);
  } else {
    (*map)[key] = value;
  }
}
}  // namespace

bool TextClassifier::ModelClickContextScoreChunks(
    int num_tokens, const TokenSpan& span_of_interest,
    const CachedFeatures& cached_features,
    tflite::Interpreter* selection_interpreter,
    std::vector<ScoredChunk>* scored_chunks) const {
  const int max_batch_size = model_->selection_options()->batch_size();

  std::vector<float> all_features;
  std::map<TokenSpan, float> chunk_scores;
  for (int batch_start = span_of_interest.first;
       batch_start < span_of_interest.second; batch_start += max_batch_size) {
    const int batch_end =
        std::min(batch_start + max_batch_size, span_of_interest.second);

    // Prepare features for the whole batch.
    all_features.clear();
    all_features.reserve(max_batch_size * cached_features.OutputFeaturesSize());
    for (int click_pos = batch_start; click_pos < batch_end; ++click_pos) {
      cached_features.AppendClickContextFeaturesForClick(click_pos,
                                                         &all_features);
    }

    // Run batched inference.
    const int batch_size = batch_end - batch_start;
    const int features_size = cached_features.OutputFeaturesSize();
    TensorView<float> logits = selection_executor_->ComputeLogits(
        TensorView<float>(all_features.data(), {batch_size, features_size}),
        selection_interpreter);
    if (!logits.is_valid()) {
      TC_LOG(ERROR) << "Couldn't compute logits.";
      return false;
    }
    if (logits.dims() != 2 || logits.dim(0) != batch_size ||
        logits.dim(1) !=
            selection_feature_processor_->GetSelectionLabelCount()) {
      TC_LOG(ERROR) << "Mismatching output.";
      return false;
    }

    // Save results.
    for (int click_pos = batch_start; click_pos < batch_end; ++click_pos) {
      const std::vector<float> scores = ComputeSoftmax(
          logits.data() + logits.dim(1) * (click_pos - batch_start),
          logits.dim(1));
      for (int j = 0;
           j < selection_feature_processor_->GetSelectionLabelCount(); ++j) {
        TokenSpan relative_token_span;
        if (!selection_feature_processor_->LabelToTokenSpan(
                j, &relative_token_span)) {
          TC_LOG(ERROR) << "Couldn't map the label to a token span.";
          return false;
        }
        const TokenSpan candidate_span = ExpandTokenSpan(
            SingleTokenSpan(click_pos), relative_token_span.first,
            relative_token_span.second);
        if (candidate_span.first >= 0 && candidate_span.second <= num_tokens) {
          UpdateMax(&chunk_scores, candidate_span, scores[j]);
        }
      }
    }
  }

  scored_chunks->clear();
  scored_chunks->reserve(chunk_scores.size());
  for (const auto& entry : chunk_scores) {
    scored_chunks->push_back(ScoredChunk{entry.first, entry.second});
  }

  return true;
}

bool TextClassifier::ModelBoundsSensitiveScoreChunks(
    int num_tokens, const TokenSpan& span_of_interest,
    const TokenSpan& inference_span, const CachedFeatures& cached_features,
    tflite::Interpreter* selection_interpreter,
    std::vector<ScoredChunk>* scored_chunks) const {
  const int max_selection_span =
      selection_feature_processor_->GetOptions()->max_selection_span();
  const int max_chunk_length = selection_feature_processor_->GetOptions()
                                       ->selection_reduced_output_space()
                                   ? max_selection_span + 1
                                   : 2 * max_selection_span + 1;
  const bool score_single_token_spans_as_zero =
      selection_feature_processor_->GetOptions()
          ->bounds_sensitive_features()
          ->score_single_token_spans_as_zero();

  scored_chunks->clear();
  if (score_single_token_spans_as_zero) {
    scored_chunks->reserve(TokenSpanSize(span_of_interest));
  }

  // Prepare all chunk candidates into one batch:
  //   - Are contained in the inference span
  //   - Have a non-empty intersection with the span of interest
  //   - Are at least one token long
  //   - Are not longer than the maximum chunk length
  std::vector<TokenSpan> candidate_spans;
  for (int start = inference_span.first; start < span_of_interest.second;
       ++start) {
    const int leftmost_end_index = std::max(start, span_of_interest.first) + 1;
    for (int end = leftmost_end_index;
         end <= inference_span.second && end - start <= max_chunk_length;
         ++end) {
      const TokenSpan candidate_span = {start, end};
      if (score_single_token_spans_as_zero &&
          TokenSpanSize(candidate_span) == 1) {
        // Do not include the single token span in the batch, add a zero score
        // for it directly to the output.
        scored_chunks->push_back(ScoredChunk{candidate_span, 0.0f});
      } else {
        candidate_spans.push_back(candidate_span);
      }
    }
  }

  const int max_batch_size = model_->selection_options()->batch_size();

  std::vector<float> all_features;
  scored_chunks->reserve(scored_chunks->size() + candidate_spans.size());
  for (int batch_start = 0; batch_start < candidate_spans.size();
       batch_start += max_batch_size) {
    const int batch_end = std::min(batch_start + max_batch_size,
                                   static_cast<int>(candidate_spans.size()));

    // Prepare features for the whole batch.
    all_features.clear();
    all_features.reserve(max_batch_size * cached_features.OutputFeaturesSize());
    for (int i = batch_start; i < batch_end; ++i) {
      cached_features.AppendBoundsSensitiveFeaturesForSpan(candidate_spans[i],
                                                           &all_features);
    }

    // Run batched inference.
    const int batch_size = batch_end - batch_start;
    const int features_size = cached_features.OutputFeaturesSize();
    TensorView<float> logits = selection_executor_->ComputeLogits(
        TensorView<float>(all_features.data(), {batch_size, features_size}),
        selection_interpreter);
    if (!logits.is_valid()) {
      TC_LOG(ERROR) << "Couldn't compute logits.";
      return false;
    }
    if (logits.dims() != 2 || logits.dim(0) != batch_size ||
        logits.dim(1) != 1) {
      TC_LOG(ERROR) << "Mismatching output.";
      return false;
    }

    // Save results.
    for (int i = batch_start; i < batch_end; ++i) {
      scored_chunks->push_back(
          ScoredChunk{candidate_spans[i], logits.data()[i - batch_start]});
    }
  }

  return true;
}

bool TextClassifier::DatetimeChunk(const UnicodeText& context_unicode,
                                   int64 reference_time_ms_utc,
                                   const std::string& reference_timezone,
                                   const std::string& locales, ModeFlag mode,
                                   std::vector<AnnotatedSpan>* result) const {
  std::vector<DatetimeParseResultSpan> datetime_spans;
  if (!datetime_parser_->Parse(context_unicode, reference_time_ms_utc,
                               reference_timezone, locales, mode,
                               &datetime_spans)) {
    return false;
  }
  for (const DatetimeParseResultSpan& datetime_span : datetime_spans) {
    AnnotatedSpan annotated_span;
    annotated_span.span = datetime_span.span;
    annotated_span.classification = {{kDateCollection,
                                      datetime_span.target_classification_score,
                                      datetime_span.priority_score}};
    annotated_span.classification[0].datetime_parse_result = datetime_span.data;

    result->push_back(std::move(annotated_span));
  }
  return true;
}

const Model* ViewModel(const void* buffer, int size) {
  if (!buffer) {
    return nullptr;
  }

  return LoadAndVerifyModel(buffer, size);
}

}  // namespace libtextclassifier2
