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

#ifndef LIBTEXTCLASSIFIER_UTILS_VARIANT_H_
#define LIBTEXTCLASSIFIER_UTILS_VARIANT_H_

#include <map>
#include <string>
#include <vector>

#include "utils/base/integral_types.h"
#include "utils/base/logging.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

// Represents a type-tagged union of different basic types.
class Variant {
 public:
  enum Type {
    TYPE_EMPTY = 0,
    TYPE_INT8_VALUE = 1,
    TYPE_UINT8_VALUE = 2,
    TYPE_INT_VALUE = 3,
    TYPE_UINT_VALUE = 4,
    TYPE_INT64_VALUE = 5,
    TYPE_UINT64_VALUE = 6,
    TYPE_FLOAT_VALUE = 7,
    TYPE_DOUBLE_VALUE = 8,
    TYPE_BOOL_VALUE = 9,
    TYPE_STRING_VALUE = 10,
    TYPE_STRING_VECTOR_VALUE = 11,
    TYPE_FLOAT_VECTOR_VALUE = 12,
    TYPE_INT_VECTOR_VALUE = 13,
  };

  Variant() : type_(TYPE_EMPTY) {}
  explicit Variant(const int8_t value)
      : type_(TYPE_INT8_VALUE), int8_value_(value) {}
  explicit Variant(const uint8_t value)
      : type_(TYPE_UINT8_VALUE), uint8_value_(value) {}
  explicit Variant(const int value)
      : type_(TYPE_INT_VALUE), int_value_(value) {}
  explicit Variant(const uint value)
      : type_(TYPE_UINT_VALUE), uint_value_(value) {}
  explicit Variant(const int64 value)
      : type_(TYPE_INT64_VALUE), long_value_(value) {}
  explicit Variant(const uint64 value)
      : type_(TYPE_UINT64_VALUE), ulong_value_(value) {}
  explicit Variant(const float value)
      : type_(TYPE_FLOAT_VALUE), float_value_(value) {}
  explicit Variant(const double value)
      : type_(TYPE_DOUBLE_VALUE), double_value_(value) {}
  explicit Variant(const StringPiece value)
      : type_(TYPE_STRING_VALUE), string_value_(value.ToString()) {}
  explicit Variant(const std::string value)
      : type_(TYPE_STRING_VALUE), string_value_(value) {}
  explicit Variant(const char* value)
      : type_(TYPE_STRING_VALUE), string_value_(value) {}
  explicit Variant(const bool value)
      : type_(TYPE_BOOL_VALUE), bool_value_(value) {}
  explicit Variant(const std::vector<std::string>& value)
      : type_(TYPE_STRING_VECTOR_VALUE), string_vector_value_(value) {}
  explicit Variant(const std::vector<float>& value)
      : type_(TYPE_FLOAT_VECTOR_VALUE), float_vector_value_(value) {}
  explicit Variant(const std::vector<int>& value)
      : type_(TYPE_INT_VECTOR_VALUE), int_vector_value_(value) {}

  Variant& operator=(const Variant&) = default;

  int Int8Value() const {
    TC3_CHECK(HasInt8());
    return int8_value_;
  }

  int UInt8Value() const {
    TC3_CHECK(HasUInt8());
    return uint8_value_;
  }

  int IntValue() const {
    TC3_CHECK(HasInt());
    return int_value_;
  }

  uint UIntValue() const {
    TC3_CHECK(HasUInt());
    return uint_value_;
  }

  int64 Int64Value() const {
    TC3_CHECK(HasInt64());
    return long_value_;
  }

  uint64 UInt64Value() const {
    TC3_CHECK(HasUInt64());
    return ulong_value_;
  }

  float FloatValue() const {
    TC3_CHECK(HasFloat());
    return float_value_;
  }

  double DoubleValue() const {
    TC3_CHECK(HasDouble());
    return double_value_;
  }

  bool BoolValue() const {
    TC3_CHECK(HasBool());
    return bool_value_;
  }

  const std::string& StringValue() const {
    TC3_CHECK(HasString());
    return string_value_;
  }

  const std::vector<std::string>& StringVectorValue() const {
    TC3_CHECK(HasStringVector());
    return string_vector_value_;
  }

  const std::vector<float>& FloatVectorValue() const {
    TC3_CHECK(HasFloatVector());
    return float_vector_value_;
  }

  const std::vector<int>& IntVectorValue() const {
    TC3_CHECK(HasIntVector());
    return int_vector_value_;
  }

  // Converts the value of this variant to its string representation, regardless
  // of the type of the actual value.
  std::string ToString() const;

  bool HasInt8() const { return type_ == TYPE_INT8_VALUE; }

  bool HasUInt8() const { return type_ == TYPE_UINT8_VALUE; }

  bool HasInt() const { return type_ == TYPE_INT_VALUE; }

  bool HasUInt() const { return type_ == TYPE_UINT_VALUE; }

  bool HasInt64() const { return type_ == TYPE_INT64_VALUE; }

  bool HasUInt64() const { return type_ == TYPE_UINT64_VALUE; }

  bool HasFloat() const { return type_ == TYPE_FLOAT_VALUE; }

  bool HasDouble() const { return type_ == TYPE_DOUBLE_VALUE; }

  bool HasBool() const { return type_ == TYPE_BOOL_VALUE; }

  bool HasString() const { return type_ == TYPE_STRING_VALUE; }

  bool HasStringVector() const { return type_ == TYPE_STRING_VECTOR_VALUE; }

  bool HasFloatVector() const { return type_ == TYPE_FLOAT_VECTOR_VALUE; }

  bool HasIntVector() const { return type_ == TYPE_INT_VECTOR_VALUE; }

  Type GetType() const { return type_; }

  bool HasValue() const { return type_ != TYPE_EMPTY; }

 private:
  Type type_;
  union {
    int8_t int8_value_;
    uint8_t uint8_value_;
    int int_value_;
    uint uint_value_;
    int64 long_value_;
    uint64 ulong_value_;
    float float_value_;
    double double_value_;
    bool bool_value_;
  };
  std::string string_value_;
  std::vector<std::string> string_vector_value_;
  std::vector<float> float_vector_value_;
  std::vector<int> int_vector_value_;
};

// Pretty-printing function for Variant.
logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const Variant& value);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_VARIANT_H_
