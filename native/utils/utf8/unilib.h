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

#ifndef LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_H_
#define LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_H_

#include "utils/base/integral_types.h"
#include "utils/utf8/unicodetext.h"

#include "utils/utf8/unilib-javaicu.h"
#define INIT_UNILIB_FOR_TESTING(VAR) VAR(nullptr)

namespace libtextclassifier3 {

class UniLib : public UniLibBase {
 public:
  using UniLibBase::UniLibBase;

  // Lowercase a unicode string.
  UnicodeText ToLowerText(const UnicodeText& text) const {
    UnicodeText result;
    for (const char32 codepoint : text) {
      result.push_back(ToLower(codepoint));
    }
    return result;
  }

  // Uppercase a unicode string.
  UnicodeText ToUpperText(const UnicodeText& text) const {
    UnicodeText result;
    for (const char32 codepoint : text) {
      result.push_back(UniLibBase::ToUpper(codepoint));
    }
    return result;
  }

  bool IsDigits(const UnicodeText& text) const {
    for (const char32 codepoint : text) {
      if (!IsDigit(codepoint)) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace libtextclassifier3
#endif  // LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_H_
