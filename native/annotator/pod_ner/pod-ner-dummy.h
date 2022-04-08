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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_POD_NER_POD_NER_DUMMY_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_POD_NER_POD_NER_DUMMY_H_

#include <memory>

#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

// Dummy version of POD NER annotator. To be included in builds that do not
// want POD NER support.
class PodNerAnnotator {
 public:
  static std::unique_ptr<PodNerAnnotator> Create(const PodNerModel *model,
                                                 const UniLib &unilib) {
    return nullptr;
  }

  bool Annotate(const UnicodeText &context,
                std::vector<AnnotatedSpan> *results) const {
    return true;
  }

  bool SuggestSelection(const UnicodeText &context, CodepointSpan click,
                        AnnotatedSpan *result) const {
    return {};
  }

  bool ClassifyText(const UnicodeText &context, CodepointSpan click,
                    ClassificationResult *result) const {
    return false;
  }

  std::vector<std::string> GetSupportedCollections() const { return {}; }
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_POD_NER_POD_NER_DUMMY_H_
