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

#ifndef LIBTEXTCLASSIFIER_SMARTSELECT_TOKEN_FEATURE_EXTRACTOR_H_
#define LIBTEXTCLASSIFIER_SMARTSELECT_TOKEN_FEATURE_EXTRACTOR_H_

#include <memory>
#include <unordered_set>
#include <vector>

#include "smartselect/types.h"
#include "util/strings/stringpiece.h"
#ifndef LIBTEXTCLASSIFIER_DISABLE_ICU_SUPPORT
#include "unicode/regex.h"
#endif

namespace libtextclassifier {

struct TokenFeatureExtractorOptions {
  // Number of buckets used for hashing charactergrams.
  int num_buckets = 0;

  // Orders of charactergrams to extract. E.g., 2 means character bigrams, 3
  // character trigrams etc.
  std::vector<int> chargram_orders;

  // Whether to extract the token case feature.
  bool extract_case_feature = false;

  // If true, will use the unicode-aware functionality for extracting features.
  bool unicode_aware_features = false;

  // Whether to extract the selection mask feature.
  bool extract_selection_mask_feature = false;

  // Regexp features to extract.
  std::vector<std::string> regexp_features;

  // Whether to remap digits to a single number.
  bool remap_digits = false;

  // Whether to lowercase all tokens.
  bool lowercase_tokens = false;

  // Maximum length of a word.
  int max_word_length = 20;

  // List of allowed charactergrams. The extracted charactergrams are filtered
  // using this list, and charactergrams that are not present are interpreted as
  // out-of-vocabulary.
  // If no allowed_chargrams are specified, all charactergrams are allowed.
  std::unordered_set<std::string> allowed_chargrams;
};

class TokenFeatureExtractor {
 public:
  explicit TokenFeatureExtractor(const TokenFeatureExtractorOptions& options);

  // Extracts features from a token.
  //  - is_in_span is a bool indicator whether the token is a part of the
  //      selection span (true) or not (false).
  //  - sparse_features are indices into a sparse feature vector of size
  //    options.num_buckets which are set to 1.0 (others are implicitly 0.0).
  //  - dense_features are values of a dense feature vector of size 0-2
  //    (depending on the options) for the token
  bool Extract(const Token& token, bool is_in_span,
               std::vector<int>* sparse_features,
               std::vector<float>* dense_features) const;

  int DenseFeaturesCount() const {
    int feature_count =
        options_.extract_case_feature + options_.extract_selection_mask_feature;
#ifndef LIBTEXTCLASSIFIER_DISABLE_ICU_SUPPORT
    feature_count += regex_patterns_.size();
#else
    if (enable_all_caps_feature_) {
      feature_count += 1;
    }
#endif
    return feature_count;
  }

 protected:
  // Hashes given token to given number of buckets.
  int HashToken(StringPiece token) const;

  // Extracts the charactergram features from the token.
  std::vector<int> ExtractCharactergramFeatures(const Token& token) const;

  // Extracts the charactergram features from the token in a non-unicode-aware
  // way.
  std::vector<int> ExtractCharactergramFeaturesAscii(const Token& token) const;

  // Extracts the charactergram features from the token in a unicode-aware way.
  std::vector<int> ExtractCharactergramFeaturesUnicode(
      const Token& token) const;

 private:
  TokenFeatureExtractorOptions options_;
#ifndef LIBTEXTCLASSIFIER_DISABLE_ICU_SUPPORT
  std::vector<std::unique_ptr<icu::RegexPattern>> regex_patterns_;
#else
  bool enable_all_caps_feature_ = false;
#endif
};

}  // namespace libtextclassifier

#endif  // LIBTEXTCLASSIFIER_SMARTSELECT_TOKEN_FEATURE_EXTRACTOR_H_
