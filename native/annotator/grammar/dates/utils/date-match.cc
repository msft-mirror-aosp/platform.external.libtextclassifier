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

#include "annotator/grammar/dates/utils/date-match.h"

#include <algorithm>

#include "annotator/grammar/dates/utils/date-utils.h"
#include "utils/strings/append.h"

namespace libtextclassifier3 {
namespace dates {

using strings::JoinStrings;
using strings::SStringAppendF;

std::string DateMatch::DebugString() const {
  std::string res;
#if !defined(NDEBUG)
  if (begin >= 0 && end >= 0) {
    SStringAppendF(&res, 0, "[%u,%u)", begin, end);
  }

  if (HasDayOfWeek()) {
    SStringAppendF(&res, 0, "%u", day_of_week);
  }

  if (HasYear()) {
    int year_output = year;
    if (HasBcAd() && bc_ad == BCAD_BC) {
      year_output = -year;
    }
    SStringAppendF(&res, 0, "%u/", year_output);
  } else {
    SStringAppendF(&res, 0, "____/");
  }

  if (HasMonth()) {
    SStringAppendF(&res, 0, "%u/", month);
  } else {
    SStringAppendF(&res, 0, "__/");
  }

  if (HasDay()) {
    SStringAppendF(&res, 0, "%u ", day);
  } else {
    SStringAppendF(&res, 0, "__ ");
  }

  if (HasHour()) {
    SStringAppendF(&res, 0, "%u:", hour);
  } else {
    SStringAppendF(&res, 0, "__:");
  }

  if (HasMinute()) {
    SStringAppendF(&res, 0, "%u:", minute);
  } else {
    SStringAppendF(&res, 0, "__:");
  }

  if (HasSecond()) {
    if (HasFractionSecond()) {
      SStringAppendF(&res, 0, "%u.%lf ", second, fraction_second);
    } else {
      SStringAppendF(&res, 0, "%u ", second);
    }
  } else {
    SStringAppendF(&res, 0, "__ ");
  }

  if (HasTimeSpanCode() && TimespanCode_TIMESPAN_CODE_NONE < time_span_code &&
      time_span_code <= TimespanCode_MAX) {
    SStringAppendF(&res, 0, "TS=%u ", time_span_code);
  }

  if (HasTimeZoneCode() && time_zone_code != -1) {
    SStringAppendF(&res, 0, "TZ= %u ", time_zone_code);
  }

  if (HasTimeZoneOffset()) {
    SStringAppendF(&res, 0, "TZO=%u ", time_zone_offset);
  }

  if (HasRelativeDate()) {
    const RelativeMatch* rm = relative_match;
    SStringAppendF(&res, 0, (rm->is_future_date ? "future " : "past "));
    if (rm->day_of_week != NO_VAL) {
      SStringAppendF(&res, 0, "DOW:%d ", rm->day_of_week);
    }
    if (rm->year != NO_VAL) {
      SStringAppendF(&res, 0, "Y:%d ", rm->year);
    }
    if (rm->month != NO_VAL) {
      SStringAppendF(&res, 0, "M:%d ", rm->month);
    }
    if (rm->day != NO_VAL) {
      SStringAppendF(&res, 0, "D:%d ", rm->day);
    }
    if (rm->week != NO_VAL) {
      SStringAppendF(&res, 0, "W:%d ", rm->week);
    }
    if (rm->hour != NO_VAL) {
      SStringAppendF(&res, 0, "H:%d ", rm->hour);
    }
    if (rm->minute != NO_VAL) {
      SStringAppendF(&res, 0, "M:%d ", rm->minute);
    }
    if (rm->second != NO_VAL) {
      SStringAppendF(&res, 0, "S:%d ", rm->second);
    }
  }

  SStringAppendF(&res, 0, "prio=%d ", priority);
  SStringAppendF(&res, 0, "conf-score=%lf ", annotator_priority_score);

  if (IsHourAmbiguous()) {
    std::vector<int8> values;
    GetPossibleHourValues(&values);
    std::string str_values;

    for (unsigned int i = 0; i < values.size(); ++i) {
      SStringAppendF(&str_values, 0, "%u,", values[i]);
    }
    SStringAppendF(&res, 0, "amb=%s ", str_values.c_str());
  }

  std::vector<std::string> tags;
  if (is_inferred) {
    tags.push_back("inferred");
  }
  if (!tags.empty()) {
    SStringAppendF(&res, 0, "tag=%s ", JoinStrings(",", tags).c_str());
  }
#endif  // !defined(NDEBUG)
  return res;
}

void DateMatch::GetPossibleHourValues(std::vector<int8>* values) const {
  TC3_CHECK(values != nullptr);
  values->clear();
  if (HasHour()) {
    int8 possible_hour = hour;
    values->push_back(possible_hour);
    for (int count = 1; count < ambiguous_hour_count; ++count) {
      possible_hour += ambiguous_hour_interval;
      if (possible_hour >= 24) {
        possible_hour -= 24;
      }
      values->push_back(possible_hour);
    }
  }
}

bool DateMatch::IsValid() const {
  if (!HasYear() && HasBcAd()) {
    return false;
  }
  if (!HasMonth() && HasYear() && (HasDay() || HasDayOfWeek())) {
    return false;
  }
  if (!HasDay() && HasDayOfWeek() && (HasYear() || HasMonth())) {
    return false;
  }
  if (!HasDay() && !HasDayOfWeek() && HasHour() && (HasYear() || HasMonth())) {
    return false;
  }
  if (!HasHour() && (HasMinute() || HasSecond() || HasFractionSecond())) {
    return false;
  }
  if (!HasMinute() && (HasSecond() || HasFractionSecond())) {
    return false;
  }
  if (!HasSecond() && HasFractionSecond()) {
    return false;
  }
  // Check whether day exists in a month, to exclude cases like "April 31".
  if (HasDay() && HasMonth() && day > GetLastDayOfMonth(year, month)) {
    return false;
  }
  return (HasDateFields() || HasTimeFields() || HasRelativeDate());
}

std::string DateRangeMatch::DebugString() const {
  std::string res;
  // The method is only called for debugging purposes.
#if !defined(NDEBUG)
  if (begin >= 0 && end >= 0) {
    SStringAppendF(&res, 0, "[%u,%u)\n", begin, end);
  }
  SStringAppendF(&res, 0, "from: %s \n", from.DebugString().c_str());
  SStringAppendF(&res, 0, "to: %s\n", to.DebugString().c_str());
#endif  // !defined(NDEBUG)
  return res;
}

}  // namespace dates
}  // namespace libtextclassifier3
