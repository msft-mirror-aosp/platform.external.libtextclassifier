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

#include "utils/base/prefixvarint.h"

#include "utils/base/integral_types.h"

namespace libtextclassifier3 {

const int PrefixVarint::kMax32;
const int PrefixVarint::kMax64;
const int PrefixVarint::kSlopBytes;
const int PrefixVarint::kEncode32SlopBytes;
const int PrefixVarint::kEncode64SlopBytes;

char* PrefixVarint::SafeEncode32(char* ptr, uint32 val) {
  return SafeEncode32Inline(ptr, val);
}

char* PrefixVarint::SafeEncode64(char* ptr, uint64 val) {
  return SafeEncode64Inline(ptr, val);
}

void PrefixVarint::Append32Slow(std::string* s, uint32 value) {
  size_t start = s->size();
  s->resize(start + PrefixVarint::Length32(value));
  PrefixVarint::SafeEncode32(&((*s)[start]), value);
}

void PrefixVarint::Append64Slow(std::string* s, uint64 value) {
  size_t start = s->size();
  s->resize(start + PrefixVarint::Length64(value));
  PrefixVarint::SafeEncode64(&((*s)[start]), value);
}

const char* PrefixVarint::Parse32Fallback(uint32 code, const char* ptr,
                                          uint32* val) {
  return Parse32FallbackInline(code, ptr, val);
}

const char* PrefixVarint::Parse64Fallback(uint64 code, const char* ptr,
                                          uint64* val) {
  return Parse64FallbackInline(code, ptr, val);
}

#if 0
const PrefixVarint::CodeInfo PrefixVarint::code_info_[8] = {
  {2, 0xff00}, {2, 0xff00},
  {2, 0xff00}, {2, 0xff00},
  {3, 0xffff00}, {3, 0xffff00},
  {4, 0xffffff00}, {5, 0xffffff00}
};
#endif

}  // namespace libtextclassifier3
