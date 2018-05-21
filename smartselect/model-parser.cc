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

#include "smartselect/model-parser.h"
#include "util/base/endian.h"

namespace libtextclassifier {
namespace {

// Small helper class for parsing the merged model format.
// The merged model consists of interleaved <int32 data_size, char* data>
// segments.
class MergedModelParser {
 public:
  MergedModelParser(const void* addr, const int size)
      : addr_(reinterpret_cast<const char*>(addr)), size_(size), pos_(addr_) {}

  bool ReadBytesAndAdvance(int num_bytes, const char** result) {
    const char* read_addr = pos_;
    if (Advance(num_bytes)) {
      *result = read_addr;
      return true;
    } else {
      return false;
    }
  }

  bool ReadInt32AndAdvance(int* result) {
    const char* read_addr = pos_;
    if (Advance(sizeof(int))) {
      *result =
          LittleEndian::ToHost32(*reinterpret_cast<const uint32*>(read_addr));
      return true;
    } else {
      return false;
    }
  }

  bool IsDone() { return pos_ == addr_ + size_; }

 private:
  bool Advance(int num_bytes) {
    pos_ += num_bytes;
    return pos_ <= addr_ + size_;
  }

  const char* addr_;
  const int size_;
  const char* pos_;
};

}  // namespace

bool ParseMergedModel(const void* addr, const int size,
                      const char** selection_model, int* selection_model_length,
                      const char** sharing_model, int* sharing_model_length) {
  MergedModelParser parser(addr, size);

  if (!parser.ReadInt32AndAdvance(selection_model_length)) {
    return false;
  }

  if (!parser.ReadBytesAndAdvance(*selection_model_length, selection_model)) {
    return false;
  }

  if (!parser.ReadInt32AndAdvance(sharing_model_length)) {
    return false;
  }

  if (!parser.ReadBytesAndAdvance(*sharing_model_length, sharing_model)) {
    return false;
  }

  return parser.IsDone();
}

}  // namespace libtextclassifier
