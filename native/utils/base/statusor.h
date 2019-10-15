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

#ifndef LIBTEXTCLASSIFIER_UTILS_BASE_STATUSOR_H_
#define LIBTEXTCLASSIFIER_UTILS_BASE_STATUSOR_H_

#include "utils/base/logging.h"
#include "utils/base/macros.h"
#include "utils/base/status.h"

namespace libtextclassifier3 {

// A StatusOr holds a Status (in the case of an error), or a value T.
template <typename T>
class StatusOr {
 public:
  // Has status UNKNOWN.
  inline StatusOr();

  // Builds from a non-OK status. Crashes if an OK status is specified.
  inline StatusOr(const Status& status);             // NOLINT
  inline StatusOr(const Status& status, T&& value);  // NOLINT

  // Builds from the specified value.
  inline StatusOr(const T& value);  // NOLINT
  inline StatusOr(T&& value);       // NOLINT

  // Copy constructor.
  inline StatusOr(const StatusOr& other);

  // Move constructor.
  inline StatusOr(StatusOr&& other);

  // Conversion copy constructor, T must be copy constructible from U.
  template <typename U>
  inline StatusOr(const StatusOr<U>& other);  // NOLINT

  // Assignment operator.
  inline StatusOr& operator=(const StatusOr& other);

  inline StatusOr& operator=(StatusOr&& other);

  // Conversion assignment operator, T must be assignable from U
  template <typename U>
  inline StatusOr& operator=(const StatusOr<U>& other);

  // Accessors.
  inline const Status& status() const { return status_; }

  // Shorthand for status().ok().
  inline bool ok() const { return status_.ok(); }

  // Returns value or crashes if ok() is false.
  inline const T& ValueOrDie() const& {
    if (!ok()) {
      TC3_LOG(FATAL) << "Attempting to fetch value of non-OK StatusOr: "
                     << status();
      exit(1);
    }
    return value_;
  }
  inline T& ValueOrDie() & {
    if (!ok()) {
      TC3_LOG(FATAL) << "Attempting to fetch value of non-OK StatusOr: "
                     << status();
      exit(1);
    }
    return value_;
  }
  inline const T&& ValueOrDie() const&& {
    if (!ok()) {
      TC3_LOG(FATAL) << "Attempting to fetch value of non-OK StatusOr: "
                     << status();
      exit(1);
    }
    return value_;
  }
  inline T&& ValueOrDie() && {
    if (!ok()) {
      TC3_LOG(FATAL) << "Attempting to fetch value of non-OK StatusOr: "
                     << status();
      exit(1);
    }
    return value_;
  }

  template <typename U>
  friend class StatusOr;

 private:
  Status status_;
  T value_;
};

// Implementation.

template <typename T>
inline StatusOr<T>::StatusOr() : status_(StatusCode::UNKNOWN, "") {}

template <typename T>
inline StatusOr<T>::StatusOr(const Status& status) : status_(status) {
  if (status.ok()) {
    TC3_LOG(FATAL) << "OkStatus() is not a valid argument to StatusOr";
    exit(1);
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const Status& status, T&& value)
    : status_(status), value_(std::move(value)) {
  if (status.ok()) {
    TC3_LOG(FATAL) << "OkStatus() is not a valid argument to StatusOr";
    exit(1);
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const T& value) : value_(value) {}

template <typename T>
inline StatusOr<T>::StatusOr(T&& value) : value_(std::move(value)) {}

template <typename T>
inline StatusOr<T>::StatusOr(const StatusOr& other)
    : status_(other.status_), value_(other.value_) {}

template <typename T>
inline StatusOr<T>::StatusOr(StatusOr&& other)
    : status_(other.status_), value_(std::move(other.value_)) {}

template <typename T>
template <typename U>
inline StatusOr<T>::StatusOr(const StatusOr<U>& other)
    : status_(other.status_), value_(other.value_) {}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr& other) {
  status_ = other.status_;
  if (status_.ok()) {
    value_ = other.value_;
  }
  return *this;
}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(StatusOr&& other) {
  status_ = other.status_;
  if (status_.ok()) {
    value_ = std::move(other.value_);
  }
  return *this;
}

template <typename T>
template <typename U>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr<U>& other) {
  status_ = other.status_;
  if (status_.ok()) {
    value_ = other.value_;
  }
  return *this;
}

}  // namespace libtextclassifier3

#define TC3_STATUS_MACROS_CONCAT_NAME(x, y) TC3_STATUS_MACROS_CONCAT_IMPL(x, y)
#define TC3_STATUS_MACROS_CONCAT_IMPL(x, y) x##y

// Macros that help consume StatusOr<...> return values and propagate errors.
#define TC3_ASSIGN_OR_RETURN(lhs, rexpr)                                 \
  TC3_ASSIGN_OR_RETURN_IMPL(                                             \
      TC3_STATUS_MACROS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, \
      rexpr)

#define TC3_ASSIGN_OR_RETURN_IMPL(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                              \
  if (TC3_PREDICT_FALSE(!statusor.ok())) {              \
    return statusor.status();                           \
  }                                                     \
  lhs = std::move(statusor.ValueOrDie())

#define TC3_ASSIGN_OR_RETURN_NULL(lhs, rexpr)                            \
  TC3_ASSIGN_OR_RETURN_NULL_IMPL(                                        \
      TC3_STATUS_MACROS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, \
      rexpr)

#define TC3_ASSIGN_OR_RETURN_NULL_IMPL(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                                   \
  if (TC3_PREDICT_FALSE(!statusor.ok())) {                   \
    return nullptr;                                          \
  }                                                          \
  lhs = std::move(statusor.ValueOrDie())

#define TC3_ASSIGN_OR_RETURN_FALSE(lhs, rexpr)                           \
  TC3_ASSIGN_OR_RETURN_FALSE_IMPL(                                       \
      TC3_STATUS_MACROS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, \
      rexpr)

#define TC3_ASSIGN_OR_RETURN_FALSE_IMPL(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                                    \
  if (TC3_PREDICT_FALSE(!statusor.ok())) {                    \
    return false;                                             \
  }                                                           \
  lhs = std::move(statusor.ValueOrDie())

#define TC3_ASSIGN_OR_RETURN_0(lhs, rexpr)                               \
  TC3_ASSIGN_OR_RETURN_0_IMPL(                                           \
      TC3_STATUS_MACROS_CONCAT_NAME(_status_or_value, __COUNTER__), lhs, \
      rexpr)

#define TC3_ASSIGN_OR_RETURN_0_IMPL(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                                \
  if (TC3_PREDICT_FALSE(!statusor.ok())) {                \
    return 0;                                             \
  }                                                       \
  lhs = std::move(statusor.ValueOrDie())

#endif  // LIBTEXTCLASSIFIER_UTILS_BASE_STATUSOR_H_
