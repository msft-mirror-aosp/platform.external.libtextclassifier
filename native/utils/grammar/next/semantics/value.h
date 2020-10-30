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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_VALUE_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_VALUE_H_

#include "utils/base/logging.h"
#include "utils/flatbuffers/reflection.h"
#include "utils/strings/stringpiece.h"
#include "utils/utf8/unicodetext.h"
#include "flatbuffers/base.h"
#include "flatbuffers/reflection.h"

namespace libtextclassifier3::grammar::next {

// A semantic value as a typed, arena-allocated flatbuffer.
// This denotes the possible results of the evaluation of a semantic expression.
class SemanticValue {
 public:
  explicit SemanticValue(const reflection::Object* type, const StringPiece data)
      : base_type_(reflection::BaseType::Obj), type_(type), data_(data) {}

  explicit SemanticValue(const bool value)
      : base_type_(reflection::BaseType::Bool),
        type_(nullptr),
        bool_value_(value) {}

  explicit SemanticValue(const int8 value)
      : base_type_(reflection::BaseType::Byte),
        type_(nullptr),
        int8_value_(value) {}

  explicit SemanticValue(const uint8 value)
      : base_type_(reflection::BaseType::UByte),
        type_(nullptr),
        uint8_value_(value) {}

  explicit SemanticValue(const int16 value)
      : base_type_(reflection::BaseType::Short),
        type_(nullptr),
        int16_value_(value) {}

  explicit SemanticValue(const uint16 value)
      : base_type_(reflection::BaseType::UShort),
        type_(nullptr),
        uint16_value_(value) {}

  explicit SemanticValue(const int32 value)
      : base_type_(reflection::BaseType::Int),
        type_(nullptr),
        int32_value_(value) {}

  explicit SemanticValue(const uint32 value)
      : base_type_(reflection::BaseType::UInt),
        type_(nullptr),
        uint32_value_(value) {}

  explicit SemanticValue(const int64 value)
      : base_type_(reflection::BaseType::Long),
        type_(nullptr),
        int64_value_(value) {}

  explicit SemanticValue(const uint64 value)
      : base_type_(reflection::BaseType::ULong),
        type_(nullptr),
        uint64_value_(value) {}

  explicit SemanticValue(const float value)
      : base_type_(reflection::BaseType::Float),
        type_(nullptr),
        float_value_(value) {}

  explicit SemanticValue(const double value)
      : base_type_(reflection::BaseType::Double),
        type_(nullptr),
        double_value_(value) {}

  explicit SemanticValue(const StringPiece value)
      : base_type_(reflection::BaseType::String),
        type_(nullptr),
        data_(value) {}

  explicit SemanticValue(const UnicodeText& value)
      : base_type_(reflection::BaseType::String),
        type_(nullptr),
        data_(StringPiece(value.data(), value.size_bytes())) {}

  template <typename T>
  bool Has() const {
    return base_type_ == libtextclassifier3::flatbuffers_base_type<T>::value;
  }

  template <>
  bool Has<flatbuffers::Table>() const {
    return base_type_ == reflection::BaseType::Obj;
  }

  template <typename T = flatbuffers::Table>
  const T* Table() const {
    TC3_CHECK(Has<flatbuffers::Table>());
    return flatbuffers::GetRoot<T>(
        reinterpret_cast<const unsigned char*>(data_.data()));
  }

  template <typename T>
  const T Value() const;

  template <>
  const bool Value<bool>() const {
    TC3_CHECK(Has<bool>());
    return bool_value_;
  }

  template <>
  const int8 Value<int8>() const {
    TC3_CHECK(Has<int8>());
    return int8_value_;
  }

  template <>
  const uint8 Value<uint8>() const {
    TC3_CHECK(Has<uint8>());
    return uint8_value_;
  }

  template <>
  const int16 Value<int16>() const {
    TC3_CHECK(Has<int16>());
    return int16_value_;
  }

  template <>
  const uint16 Value<uint16>() const {
    TC3_CHECK(Has<uint16>());
    return uint16_value_;
  }

  template <>
  const int32 Value<int32>() const {
    TC3_CHECK(Has<int32>());
    return int32_value_;
  }

  template <>
  const uint32 Value<uint32>() const {
    TC3_CHECK(Has<uint32>());
    return uint32_value_;
  }

  template <>
  const int64 Value<int64>() const {
    TC3_CHECK(Has<int64>());
    return int64_value_;
  }

  template <>
  const uint64 Value<uint64>() const {
    TC3_CHECK(Has<uint64>());
    return uint64_value_;
  }

  template <>
  const float Value<float>() const {
    TC3_CHECK(Has<float>());
    return float_value_;
  }

  template <>
  const double Value<double>() const {
    TC3_CHECK(Has<double>());
    return double_value_;
  }

  template <>
  const StringPiece Value<StringPiece>() const {
    TC3_CHECK(Has<StringPiece>());
    return data_;
  }

  template <>
  const std::string Value<std::string>() const {
    TC3_CHECK(Has<StringPiece>());
    return data_.ToString();
  }

  template <>
  const UnicodeText Value<UnicodeText>() const {
    TC3_CHECK(Has<StringPiece>());
    return UTF8ToUnicodeText(data_, /*do_copy=*/false);
  }

  const reflection::BaseType base_type() const { return base_type_; }
  const reflection::Object* type() const { return type_; }

 private:
  // The base type.
  const reflection::BaseType base_type_;

  // The object type of the value.
  const reflection::Object* type_;

  // The data payload.
  union {
    int8 int8_value_;
    uint8 uint8_value_;
    int16 int16_value_;
    uint16 uint16_value_;
    int32 int32_value_;
    uint32 uint32_value_;
    int64 int64_value_;
    uint64 uint64_value_;
    float float_value_;
    double double_value_;
    bool bool_value_;
  };
  StringPiece data_;
};

}  // namespace libtextclassifier3::grammar::next

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_VALUE_H_
