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

#ifndef LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_COMMON_H_
#define LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_COMMON_H_

#include "annotator/types.h"
#include "utils/base/integral_types.h"
#include "utils/base/logging.h"
#include "utils/base/macros.h"

namespace libtextclassifier3 {
namespace calendar {

// Macro to reduce the amount of boilerplate needed for propagating errors.
#define TC3_CALENDAR_CHECK(EXPR) \
  if (!(EXPR)) {                 \
    return false;                \
  }

// An implementation of CalendarLib that is independent of the particular
// calendar implementation used (implementation type is passed as template
// argument).
template <class TCalendar>
class CalendarLibTempl {
 public:
  bool InterpretParseData(const DateParseData& parse_data,
                          int64 reference_time_ms_utc,
                          const std::string& reference_timezone,
                          const std::string& reference_locale,
                          DatetimeGranularity granularity,
                          TCalendar* calendar) const;

 private:
  // Adjusts the calendar's time instant according to a relative date reference
  // in the parsed data.
  bool ApplyRelationField(const DateParseData& parse_data,
                          TCalendar* calendar) const;

  // Round the time instant's precision down to the given granularity.
  bool RoundToGranularity(DatetimeGranularity granularity,
                          TCalendar* calendar) const;

  // Adjusts time in steps of relation_type, by distance steps.
  // For example:
  // - Adjusting by -2 MONTHS will return the beginning of the 1st
  //   two weeks ago.
  // - Adjusting by +4 Wednesdays will return the beginning of the next
  //   Wednesday at least 4 weeks from now.
  // If allow_today is true, the same day of the week may be kept
  // if it already matches the relation type.
  bool AdjustByRelation(DateParseData::RelationType relation_type, int distance,
                        bool allow_today, TCalendar* calendar) const;
};

template <class TCalendar>
bool CalendarLibTempl<TCalendar>::InterpretParseData(
    const DateParseData& parse_data, int64 reference_time_ms_utc,
    const std::string& reference_timezone, const std::string& reference_locale,
    DatetimeGranularity granularity, TCalendar* calendar) const {
  TC3_CALENDAR_CHECK(calendar->Initialize(reference_timezone, reference_locale,
                                          reference_time_ms_utc))

  // By default, the parsed time is interpreted to be on the reference day.
  // But a parsed date should have time 0:00:00 unless specified.
  TC3_CALENDAR_CHECK(calendar->SetHourOfDay(0))
  TC3_CALENDAR_CHECK(calendar->SetMinute(0))
  TC3_CALENDAR_CHECK(calendar->SetSecond(0))
  TC3_CALENDAR_CHECK(calendar->SetMillisecond(0))

  // Apply each of the parsed fields in order of increasing granularity.
  static const int64 kMillisInHour = 1000 * 60 * 60;
  if (parse_data.field_set_mask & DateParseData::Fields::ZONE_OFFSET_FIELD) {
    TC3_CALENDAR_CHECK(
        calendar->SetZoneOffset(parse_data.zone_offset * kMillisInHour))
  }
  if (parse_data.field_set_mask & DateParseData::Fields::DST_OFFSET_FIELD) {
    TC3_CALENDAR_CHECK(
        calendar->SetDstOffset(parse_data.dst_offset * kMillisInHour))
  }
  if (parse_data.field_set_mask & DateParseData::Fields::RELATION_FIELD) {
    TC3_CALENDAR_CHECK(ApplyRelationField(parse_data, calendar));
  }
  if (parse_data.field_set_mask & DateParseData::Fields::YEAR_FIELD) {
    TC3_CALENDAR_CHECK(calendar->SetYear(parse_data.year))
  }
  if (parse_data.field_set_mask & DateParseData::Fields::MONTH_FIELD) {
    // ICU has months starting at 0, Java and Datetime parser at 1, so we
    // need to subtract 1.
    TC3_CALENDAR_CHECK(calendar->SetMonth(parse_data.month - 1))
  }
  if (parse_data.field_set_mask & DateParseData::Fields::DAY_FIELD) {
    TC3_CALENDAR_CHECK(calendar->SetDayOfMonth(parse_data.day_of_month))
  }
  if (parse_data.field_set_mask & DateParseData::Fields::HOUR_FIELD) {
    if (parse_data.field_set_mask & DateParseData::Fields::AMPM_FIELD &&
        parse_data.ampm == DateParseData::AMPM::PM && parse_data.hour < 12) {
      TC3_CALENDAR_CHECK(calendar->SetHourOfDay(parse_data.hour + 12))
    } else {
      TC3_CALENDAR_CHECK(calendar->SetHourOfDay(parse_data.hour))
    }
  }
  if (parse_data.field_set_mask & DateParseData::Fields::MINUTE_FIELD) {
    TC3_CALENDAR_CHECK(calendar->SetMinute(parse_data.minute))
  }
  if (parse_data.field_set_mask & DateParseData::Fields::SECOND_FIELD) {
    TC3_CALENDAR_CHECK(calendar->SetSecond(parse_data.second))
  }

  TC3_CALENDAR_CHECK(RoundToGranularity(granularity, calendar))
  return true;
}

template <class TCalendar>
bool CalendarLibTempl<TCalendar>::ApplyRelationField(
    const DateParseData& parse_data, TCalendar* calendar) const {
  constexpr int relation_type_mask = DateParseData::Fields::RELATION_TYPE_FIELD;
  constexpr int relation_distance_mask =
      DateParseData::Fields::RELATION_DISTANCE_FIELD;
  switch (parse_data.relation) {
    case DateParseData::Relation::NEXT:
      if (parse_data.field_set_mask & relation_type_mask) {
        TC3_CALENDAR_CHECK(AdjustByRelation(parse_data.relation_type,
                                            /*distance=*/1,
                                            /*allow_today=*/false, calendar));
      }
      return true;
    case DateParseData::Relation::NEXT_OR_SAME:
      if (parse_data.field_set_mask & relation_type_mask) {
        TC3_CALENDAR_CHECK(AdjustByRelation(parse_data.relation_type,
                                            /*distance=*/1,
                                            /*allow_today=*/true, calendar))
      }
      return true;
    case DateParseData::Relation::LAST:
      if (parse_data.field_set_mask & relation_type_mask) {
        TC3_CALENDAR_CHECK(AdjustByRelation(parse_data.relation_type,
                                            /*distance=*/-1,
                                            /*allow_today=*/false, calendar))
      }
      return true;
    case DateParseData::Relation::NOW:
      return true;  // NOOP
    case DateParseData::Relation::TOMORROW:
      TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(1));
      return true;
    case DateParseData::Relation::YESTERDAY:
      TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(-1));
      return true;
    case DateParseData::Relation::PAST:
      if ((parse_data.field_set_mask & relation_type_mask) &&
          (parse_data.field_set_mask & relation_distance_mask)) {
        TC3_CALENDAR_CHECK(AdjustByRelation(parse_data.relation_type,
                                            -parse_data.relation_distance,
                                            /*allow_today=*/false, calendar))
      }
      return true;
    case DateParseData::Relation::FUTURE:
      if ((parse_data.field_set_mask & relation_type_mask) &&
          (parse_data.field_set_mask & relation_distance_mask)) {
        TC3_CALENDAR_CHECK(AdjustByRelation(parse_data.relation_type,
                                            parse_data.relation_distance,
                                            /*allow_today=*/false, calendar))
      }
      return true;
  }
  return false;
}

template <class TCalendar>
bool CalendarLibTempl<TCalendar>::RoundToGranularity(
    DatetimeGranularity granularity, TCalendar* calendar) const {
  // Force recomputation before doing the rounding.
  int unused;
  TC3_CALENDAR_CHECK(calendar->GetDayOfWeek(&unused));

  switch (granularity) {
    case GRANULARITY_YEAR:
      TC3_CALENDAR_CHECK(calendar->SetMonth(0));
      TC3_FALLTHROUGH_INTENDED;
    case GRANULARITY_MONTH:
      TC3_CALENDAR_CHECK(calendar->SetDayOfMonth(1));
      TC3_FALLTHROUGH_INTENDED;
    case GRANULARITY_DAY:
      TC3_CALENDAR_CHECK(calendar->SetHourOfDay(0));
      TC3_FALLTHROUGH_INTENDED;
    case GRANULARITY_HOUR:
      TC3_CALENDAR_CHECK(calendar->SetMinute(0));
      TC3_FALLTHROUGH_INTENDED;
    case GRANULARITY_MINUTE:
      TC3_CALENDAR_CHECK(calendar->SetSecond(0));
      break;

    case GRANULARITY_WEEK:
      int first_day_of_week;
      TC3_CALENDAR_CHECK(calendar->GetFirstDayOfWeek(&first_day_of_week));
      TC3_CALENDAR_CHECK(calendar->SetDayOfWeek(first_day_of_week));
      TC3_CALENDAR_CHECK(calendar->SetHourOfDay(0));
      TC3_CALENDAR_CHECK(calendar->SetMinute(0));
      TC3_CALENDAR_CHECK(calendar->SetSecond(0));
      break;

    case GRANULARITY_UNKNOWN:
    case GRANULARITY_SECOND:
      break;
  }
  return true;
}

template <class TCalendar>
bool CalendarLibTempl<TCalendar>::AdjustByRelation(
    DateParseData::RelationType relation_type, int distance, bool allow_today,
    TCalendar* calendar) const {
  const int distance_sign = distance < 0 ? -1 : 1;
  switch (relation_type) {
    case DateParseData::MONDAY:
    case DateParseData::TUESDAY:
    case DateParseData::WEDNESDAY:
    case DateParseData::THURSDAY:
    case DateParseData::FRIDAY:
    case DateParseData::SATURDAY:
    case DateParseData::SUNDAY:
      if (!allow_today) {
        // If we're not including the same day as the reference, skip it.
        TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(distance_sign))
      }
      // Keep walking back until we hit the desired day of the week.
      while (distance != 0) {
        int day_of_week;
        TC3_CALENDAR_CHECK(calendar->GetDayOfWeek(&day_of_week))
        if (day_of_week == relation_type) {
          distance += -distance_sign;
          if (distance == 0) break;
        }
        TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(distance_sign))
      }
      return true;
    case DateParseData::DAY:
      TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(distance));
      return true;
    case DateParseData::WEEK:
      TC3_CALENDAR_CHECK(calendar->AddDayOfMonth(7 * distance))
      TC3_CALENDAR_CHECK(calendar->SetDayOfWeek(1))
      return true;
    case DateParseData::MONTH:
      TC3_CALENDAR_CHECK(calendar->AddMonth(distance))
      TC3_CALENDAR_CHECK(calendar->SetDayOfMonth(1))
      return true;
    case DateParseData::YEAR:
      TC3_CALENDAR_CHECK(calendar->AddYear(distance))
      TC3_CALENDAR_CHECK(calendar->SetDayOfYear(1))
      return true;
    default:
      return false;
  }
  return false;
}

};  // namespace calendar

#undef TC3_CALENDAR_CHECK

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_COMMON_H_
