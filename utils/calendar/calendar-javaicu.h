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

#ifndef LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_JAVAICU_H_
#define LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_JAVAICU_H_

#include <jni.h>
#include <memory>
#include <string>

#include "annotator/types.h"
#include "utils/base/integral_types.h"
#include "utils/calendar/calendar-common.h"
#include "utils/java/jni-cache.h"
#include "utils/java/scoped_local_ref.h"

namespace libtextclassifier3 {

class Calendar {
 public:
  explicit Calendar(JniCache* jni_cache);
  bool Initialize(const std::string& time_zone, const std::string& locale,
                  int64 time_ms_utc);
  bool AddDayOfMonth(int value) const;
  bool AddYear(int value) const;
  bool AddMonth(int value) const;
  bool GetDayOfWeek(int* value) const;
  bool GetFirstDayOfWeek(int* value) const;
  bool GetTimeInMillis(int64* value) const;
  bool SetZoneOffset(int value) const;
  bool SetDstOffset(int value) const;
  bool SetYear(int value) const;
  bool SetMonth(int value) const;
  bool SetDayOfYear(int value) const;
  bool SetDayOfMonth(int value) const;
  bool SetDayOfWeek(int value) const;
  bool SetHourOfDay(int value) const;
  bool SetMinute(int value) const;
  bool SetSecond(int value) const;
  bool SetMillisecond(int value) const;

 private:
  JniCache* jni_cache_;
  JNIEnv* jenv_;
  ScopedLocalRef<jobject> calendar_;
};

class CalendarLib {
 public:
  CalendarLib();
  explicit CalendarLib(const std::shared_ptr<JniCache>& jni_cache);

  // Returns false (dummy version).
  bool InterpretParseData(const DateParseData& parse_data,
                          int64 reference_time_ms_utc,
                          const std::string& reference_timezone,
                          const std::string& reference_locale,
                          DatetimeGranularity granularity,
                          int64* interpreted_time_ms_utc) const {
    Calendar calendar(jni_cache_.get());
    calendar::CalendarLibTempl<Calendar> impl;
    if (!impl.InterpretParseData(parse_data, reference_time_ms_utc,
                                 reference_timezone, reference_locale,
                                 granularity, &calendar)) {
      return false;
    }
    return calendar.GetTimeInMillis(interpreted_time_ms_utc);
  }

 private:
  std::shared_ptr<JniCache> jni_cache_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_JAVAICU_H_
