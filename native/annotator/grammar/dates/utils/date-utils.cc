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

#include "annotator/grammar/dates/utils/date-utils.h"

#include <algorithm>
#include <ctime>

#include "annotator/grammar/dates/annotations/annotation-util.h"
#include "annotator/grammar/dates/dates_generated.h"
#include "annotator/grammar/dates/utils/annotation-keys.h"
#include "utils/base/macros.h"

namespace libtextclassifier3 {
namespace dates {

bool IsLeapYear(int year) {
  // For the sake of completeness, we want to be able to decide
  // whether a year is a leap year all the way back to 0 Julian, or
  // 4714 BCE. But we don't want to take the modulus of a negative
  // number, because this may not be very well-defined or portable. So
  // we increment the year by some large multiple of 400, which is the
  // periodicity of this leap-year calculation.
  if (year < 0) {
    year += 8000;
  }
  return ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0));
}

namespace {
#define SECSPERMIN (60)
#define MINSPERHOUR (60)
#define HOURSPERDAY (24)
#define DAYSPERWEEK (7)
#define DAYSPERNYEAR (365)
#define DAYSPERLYEAR (366)
#define MONSPERYEAR (12)

const int8 kDaysPerMonth[2][1 + MONSPERYEAR] = {
    {-1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {-1, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};
}  // namespace

int8 GetLastDayOfMonth(int year, int month) {
  if (year == 0) {  // No year specified
    return kDaysPerMonth[1][month];
  }
  return kDaysPerMonth[IsLeapYear(year)][month];
}

namespace {
inline bool IsHourInSegment(const TimeSpanSpec_::Segment* segment, int8 hour,
                            bool is_exact) {
  return (hour >= segment->begin() &&
          (hour < segment->end() ||
           (hour == segment->end() && is_exact && segment->is_closed())));
}

Property* FindOrCreateDefaultDateTime(AnnotationData* inst) {
  // Refer comments for kDateTime in annotation-keys.h to see the format.
  static constexpr int kDefault[] = {-1, -1, -1, -1, -1, -1, -1, -1};

  int idx = GetPropertyIndex(kDateTime, *inst);
  if (idx < 0) {
    idx = AddRepeatedIntProperty(kDateTime, kDefault, TC3_ARRAYSIZE(kDefault),
                                 inst);
  }
  return &inst->properties[idx];
}

void IncrementDayOfWeek(DayOfWeek* dow) {
  static const DayOfWeek dow_ring[] = {DayOfWeek_MONDAY,    DayOfWeek_TUESDAY,
                                       DayOfWeek_WEDNESDAY, DayOfWeek_THURSDAY,
                                       DayOfWeek_FRIDAY,    DayOfWeek_SATURDAY,
                                       DayOfWeek_SUNDAY,    DayOfWeek_MONDAY};
  const auto& cur_dow =
      std::find(std::begin(dow_ring), std::end(dow_ring), *dow);
  if (cur_dow != std::end(dow_ring)) {
    *dow = *std::next(cur_dow);
  }
}
}  // namespace

bool NormalizeHourByTimeSpan(const TimeSpanSpec* ts_spec, DateMatch* date) {
  if (ts_spec->segment() == nullptr) {
    return false;
  }
  if (date->HasHour()) {
    const bool is_exact =
        (!date->HasMinute() ||
         (date->minute == 0 &&
          (!date->HasSecond() ||
           (date->second == 0 &&
            (!date->HasFractionSecond() || date->fraction_second == 0.0)))));
    for (const TimeSpanSpec_::Segment* segment : *ts_spec->segment()) {
      if (IsHourInSegment(segment, date->hour + segment->offset(), is_exact)) {
        date->hour += segment->offset();
        return true;
      }
      if (!segment->is_strict() &&
          IsHourInSegment(segment, date->hour, is_exact)) {
        return true;
      }
    }
  } else {
    for (const TimeSpanSpec_::Segment* segment : *ts_spec->segment()) {
      if (segment->is_stand_alone()) {
        if (segment->begin() == segment->end()) {
          date->hour = segment->begin();
        }
        // Allow stand-alone time-span points and ranges.
        return true;
      }
    }
  }
  return false;
}

bool IsRefinement(const DateMatch& a, const DateMatch& b) {
  int count = 0;
  if (b.HasBcAd()) {
    if (!a.HasBcAd() || a.bc_ad != b.bc_ad) return false;
  } else if (a.HasBcAd()) {
    if (a.bc_ad == BCAD_BC) return false;
    ++count;
  }
  if (b.HasYear()) {
    if (!a.HasYear() || a.year != b.year) return false;
  } else if (a.HasYear()) {
    ++count;
  }
  if (b.HasMonth()) {
    if (!a.HasMonth() || a.month != b.month) return false;
  } else if (a.HasMonth()) {
    ++count;
  }
  if (b.HasDay()) {
    if (!a.HasDay() || a.day != b.day) return false;
  } else if (a.HasDay()) {
    ++count;
  }
  if (b.HasDayOfWeek()) {
    if (!a.HasDayOfWeek() || a.day_of_week != b.day_of_week) return false;
  } else if (a.HasDayOfWeek()) {
    ++count;
  }
  if (b.HasHour()) {
    if (!a.HasHour()) return false;
    std::vector<int8> possible_hours;
    b.GetPossibleHourValues(&possible_hours);
    if (std::find(possible_hours.begin(), possible_hours.end(), a.hour) ==
        possible_hours.end()) {
      return false;
    }
  } else if (a.HasHour()) {
    ++count;
  }
  if (b.HasMinute()) {
    if (!a.HasMinute() || a.minute != b.minute) return false;
  } else if (a.HasMinute()) {
    ++count;
  }
  if (b.HasSecond()) {
    if (!a.HasSecond() || a.second != b.second) return false;
  } else if (a.HasSecond()) {
    ++count;
  }
  if (b.HasFractionSecond()) {
    if (!a.HasFractionSecond() || a.fraction_second != b.fraction_second)
      return false;
  } else if (a.HasFractionSecond()) {
    ++count;
  }
  if (b.HasTimeSpanCode()) {
    if (!a.HasTimeSpanCode() || a.time_span_code != b.time_span_code)
      return false;
  } else if (a.HasTimeSpanCode()) {
    ++count;
  }
  if (b.HasTimeZoneCode()) {
    if (!a.HasTimeZoneCode() || a.time_zone_code != b.time_zone_code)
      return false;
  } else if (a.HasTimeZoneCode()) {
    ++count;
  }
  if (b.HasTimeZoneOffset()) {
    if (!a.HasTimeZoneOffset() || a.time_zone_offset != b.time_zone_offset)
      return false;
  } else if (a.HasTimeZoneOffset()) {
    ++count;
  }
  return (count > 0 || a.priority >= b.priority);
}

bool IsRefinement(const DateRangeMatch& a, const DateRangeMatch& b) {
  return false;
}

bool IsPrecedent(const DateMatch& a, const DateMatch& b) {
  if (a.HasYear() && b.HasYear()) {
    if (a.year < b.year) return true;
    if (a.year > b.year) return false;
  }

  if (a.HasMonth() && b.HasMonth()) {
    if (a.month < b.month) return true;
    if (a.month > b.month) return false;
  }

  if (a.HasDay() && b.HasDay()) {
    if (a.day < b.day) return true;
    if (a.day > b.day) return false;
  }

  if (a.HasHour() && b.HasHour()) {
    if (a.hour < b.hour) return true;
    if (a.hour > b.hour) return false;
  }

  if (a.HasMinute() && b.HasHour()) {
    if (a.minute < b.hour) return true;
    if (a.minute > b.hour) return false;
  }

  if (a.HasSecond() && b.HasSecond()) {
    if (a.second < b.hour) return true;
    if (a.second > b.hour) return false;
  }

  return false;
}

void IncrementOneDay(DateMatch* date) {
  if (date->HasDayOfWeek()) {
    IncrementDayOfWeek(&date->day_of_week);
  }
  if (date->HasYear() && date->HasMonth()) {
    if (date->day < GetLastDayOfMonth(date->year, date->month)) {
      date->day++;
      return;
    } else if (date->month < MONSPERYEAR) {
      date->month++;
      date->day = 1;
      return;
    } else {
      date->year++;
      date->month = 1;
      date->day = 1;
      return;
    }
  } else if (!date->HasYear() && date->HasMonth()) {
    if (date->day < GetLastDayOfMonth(0, date->month)) {
      date->day++;
      return;
    } else if (date->month < MONSPERYEAR) {
      date->month++;
      date->day = 1;
      return;
    }
  } else {
    date->day++;
    return;
  }
}

void FillDateInstance(const DateMatch& date, Annotation* instance) {
  instance->begin = date.begin;
  instance->end = date.end;
  AnnotationData* thing = &instance->data;
  thing->type = kDateTimeType;

  // Add most common date time fields. Refer kDateTime to see the format.
  auto has_value = [](int n) { return n >= 0; };
  int sec_frac = -1;
  if (date.HasFractionSecond()) {
    sec_frac = static_cast<int>(date.fraction_second * 1000);
  }
  int datetime[] = {date.year,   date.month,  date.day, date.hour,
                    date.minute, date.second, sec_frac, date.day_of_week};
  if (std::any_of(datetime, datetime + TC3_ARRAYSIZE(datetime), has_value)) {
    AddRepeatedIntProperty(kDateTime, datetime, TC3_ARRAYSIZE(datetime),
                           instance);
  }

  // Refer comments of kDateTimeSupplementary to see the format.
  int datetime_sup[] = {date.bc_ad, date.time_span_code, date.time_zone_code,
                        date.time_zone_offset};
  if (std::any_of(datetime_sup, datetime_sup + TC3_ARRAYSIZE(datetime_sup),
                  has_value)) {
    AddRepeatedIntProperty(kDateTimeSupplementary, datetime_sup,
                           TC3_ARRAYSIZE(datetime_sup), instance);
  }

  if (date.HasRelativeDate()) {
    const RelativeMatch* r_match = date.relative_match;
    // Refer comments of kDateTimeRelative to see the format.
    int is_future = -1;
    if (r_match->existing & RelativeMatch::HAS_IS_FUTURE) {
      is_future = r_match->is_future_date;
    }
    int rdate[] = {is_future,       r_match->year,   r_match->month,
                   r_match->day,    r_match->week,   r_match->hour,
                   r_match->minute, r_match->second, r_match->day_of_week};
    int idx = AddRepeatedIntProperty(kDateTimeRelative, rdate,
                                     TC3_ARRAYSIZE(rdate), instance);

    if (r_match->existing & RelativeMatch::HAS_DAY_OF_WEEK) {
      if (r_match->IsStandaloneRelativeDayOfWeek() &&
          date.day_of_week == DayOfWeek_DOW_NONE) {
        Property* prop = FindOrCreateDefaultDateTime(&instance->data);
        prop->int_values[7] = r_match->day_of_week;
      }
      // Check if the relative date has day of week with week period.
      // "Tuesday 6 weeks ago".
      if (r_match->existing & RelativeMatch::HAS_WEEK) {
        instance->data.properties[idx].int_values.push_back(
            RelativeParameter_::Interpretation_SOME);
      } else {
        const NonterminalValue* nonterminal = r_match->day_of_week_nonterminal;
        TC3_CHECK(nonterminal != nullptr);
        TC3_CHECK(nonterminal->relative_parameter());
        const RelativeParameter* rp = nonterminal->relative_parameter();
        if (rp->day_of_week_interpretation()) {
          for (const int interpretation : *rp->day_of_week_interpretation()) {
            instance->data.properties[idx].int_values.push_back(interpretation);
          }
        }
      }
    }
  }
}

void FillDateRangeInstance(const DateRangeMatch& range, Annotation* instance) {
  instance->begin = range.begin;
  instance->end = range.end;
  instance->data.type = kDateTimeRangeType;

  Annotation from_date;
  FillDateInstance(range.from, &from_date);
  AddAnnotationDataProperty(kDateTimeRangeFrom, from_date.data, instance);

  Annotation to_date;
  FillDateInstance(range.to, &to_date);
  AddAnnotationDataProperty(kDateTimeRangeTo, to_date.data, instance);
}

namespace {
int NormalizeField(int base, int zero, int* valp, int carry_in) {
  int carry_out = 0;
  int val = *valp;
  if (zero != 0 && val < 0) {
    val += base;
    carry_out -= 1;
  }
  val -= zero;
  carry_out += val / base;
  int rem = val % base;
  if (carry_in != 0) {
    carry_out += carry_in / base;
    rem += carry_in % base;
    if (rem < 0) {
      carry_out -= 1;
      rem += base;
    } else if (rem >= base) {
      carry_out += 1;
      rem -= base;
    }
  }
  if (rem < 0) {
    carry_out -= 1;
    rem += base;
  }
  *valp = rem + zero;
  return carry_out;
}

int DaysPerYear(int year) {
  if (IsLeapYear(year)) {
    return DAYSPERLYEAR;
  }
  return DAYSPERNYEAR;
}

const int8 kDaysPer100Years[401] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

int DaysPer100Years(int eyear) { return 36524 + kDaysPer100Years[eyear]; }

const int8 kDaysPer4Years[401] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

int DaysPer4Years(int eyear) { return 1460 + kDaysPer4Years[eyear]; }

#define DAYORDYEARMAX (25252734927766553LL)
#define DAYORDYEARMIN (-25252734927764584LL)

// Normalize year, month, day, hour, minute and second to valid value. For
// example:  1hour 15minute 62second is normalized as 1hour 16 minute 2second.
bool NormalizeDateFields(int* year, int* month, int* day, int* hour,
                         int* minute, int* second) {
  int min_carry = NormalizeField(SECSPERMIN, 0, second, 0);
  int hour_carry = NormalizeField(MINSPERHOUR, 0, minute, min_carry);
  int day_carry = NormalizeField(HOURSPERDAY, 0, hour, hour_carry);
  int year_carry = NormalizeField(MONSPERYEAR, 1, month, 0);
  bool normalized = min_carry || hour_carry || day_carry || year_carry;

  // Normalize the number of days within a 400-year (146097-day) period.
  if (int c4_carry = NormalizeField(146097, 1, day, day_carry)) {
    year_carry += c4_carry * 400;
    normalized = true;
  }

  // Extract a [0:399] year calendrically equivalent to (year + year_carry)
  // from that sum in order to simplify year/day normalization and to defer
  // the possibility of int64 overflow until the final stage.
  int eyear = *year % 400;
  if (year_carry != 0) {
    eyear += year_carry;
    eyear %= 400;
  }
  if (eyear < 0) eyear += 400;
  year_carry -= eyear;

  int orig_day = *day;
  if (*day > DAYSPERNYEAR) {
    eyear += (*month > 2 ? 1 : 0);
    if (*day > 146097 - DAYSPERNYEAR) {
      // We often hit the 400th year when stepping a civil time backwards,
      // so special case it to avoid counting up by 100/4/1 year chunks.
      *day = DaysPerYear(eyear += 400 - 1) - (146097 - *day);
    } else {
      // Handle days in chunks of 100/4/1 years.
      for (int ydays = DaysPer100Years(eyear); *day > ydays;
           *day -= ydays, ydays = DaysPer100Years(eyear)) {
        if ((eyear += 100) > 400) {
          eyear -= 400;
          year_carry += 400;
        }
      }
      for (int ydays = DaysPer4Years(eyear); *day > ydays;
           *day -= ydays, ydays = DaysPer4Years(eyear)) {
        if ((eyear += 4) > 400) {
          eyear -= 400;
          year_carry += 400;
        }
      }
      for (int ydays = DaysPerYear(eyear); *day > ydays;
           *day -= ydays, ydays = DaysPerYear(eyear)) {
        eyear += 1;
      }
    }
    eyear -= (*month > 2 ? 1 : 0);
  }
  // Handle days within one year.
  bool leap_year = IsLeapYear(eyear);
  for (int mdays = kDaysPerMonth[leap_year][*month]; *day > mdays;
       *day -= mdays, mdays = kDaysPerMonth[leap_year][*month]) {
    if (++*month > MONSPERYEAR) {
      *month = 1;
      leap_year = IsLeapYear(++eyear);
    }
  }
  if (*day != orig_day) normalized = true;

  // Add the updated eyear back into (year + year_carry).
  year_carry += eyear;
  // Overflow.
  if (*year > DAYORDYEARMAX - year_carry) {
    return false;
  } else if (*year < DAYORDYEARMIN - year_carry) {
    return false;
  }
  *year += year_carry;
  return true;
}

// Compute the day difference between the day of week in relative date and wday.
// If the relative date is in future, return positive days. otherwise return the
// negative future. For example:
// if day of week in relative date is Mon this week and wday is Wed this week,
// then return -2.
// if day of week in relative date is Wed this week and wday is Mon this week,
// then return 2.
int32 RelativeDOWToDays(const Property& rd, const int wday) {
  int days = -1;
  int multiplier = 1;
  for (int i = 9; i < rd.int_values.size(); ++i) {
    int inter = rd.int_values.at(i);
    int dow = rd.int_values.at(8) - 1;
    int interval = 0;
    int cur_multiplier = 1;
    if (inter == RelativeParameter_::Interpretation_NEAREST_LAST ||
        inter == RelativeParameter_::Interpretation_PREVIOUS) {
      // Represent the DOW in the last week.
      cur_multiplier = -1;
      if (dow <= wday) {
        interval = 7 + (wday - dow);
      } else {
        interval = 7 - (dow - wday);
      }
    } else if (inter == RelativeParameter_::Interpretation_SECOND_LAST) {
      // Represent the DOW in the week before last week.
      cur_multiplier = -1;
      if (dow <= wday) {
        interval = 14 + (wday - dow);
      } else {
        interval = 14 - (dow - wday);
      }
    } else if (inter == RelativeParameter_::Interpretation_NEAREST_NEXT ||
               inter == RelativeParameter_::Interpretation_COMING) {
      // Represent the DOW in the next week.
      cur_multiplier = 1;
      if (dow <= wday) {
        interval = 7 - (wday - dow);
      } else {
        interval = 7 + (dow - wday);
      }
      // Represent the DOW in the week of next week.
    } else if (inter == RelativeParameter_::Interpretation_SECOND_NEXT) {
      cur_multiplier = 1;
      if (dow <= wday) {
        interval = 14 - (wday - dow);
      } else {
        interval = 14 + (dow - wday);
      }
      // Represent the DOW in the same week regardless of it's past of future.
    } else if (inter == RelativeParameter_::Interpretation_CURRENT ||
               inter == RelativeParameter_::Interpretation_NEAREST ||
               inter == RelativeParameter_::Interpretation_SOME) {
      interval = abs(wday - dow);
      cur_multiplier = dow < wday ? -1 : 1;
    }
    if (days == -1 || interval < days) {
      days = interval;
      multiplier = cur_multiplier;
    }
  }
  return days * multiplier;
}

// Compute the absolute date and time based on timestamp and relative date and
// fill the fields year, month, day, hour, minute and second.
bool RelativeDateToAbsoluteDate(struct tm ts, AnnotationData* date) {
  int idx = GetPropertyIndex(kDateTimeRelative, *date);
  if (idx < 0) {
    return false;
  }
  Property* datetime = FindOrCreateDefaultDateTime(date);
  Property* relative = &date->properties[idx];
  int year = ts.tm_year + 1900;  // The year in struct tm is since 1900
  int month = ts.tm_mon + 1;     // Convert to [1, 12]
  int day = ts.tm_mday;
  int hour = ts.tm_hour;
  int minute = ts.tm_min;
  int second = ts.tm_sec;
  // If the instance has time, it doesn't make sense to update time based on
  // relative time. so we simply clear the time in relative date.
  // For example: 2 days 1 hours ago at 10:00am, the 1 hours will be removed.
  if (datetime->int_values[3] > 0) {
    relative->int_values[5] = -1;
    relative->int_values[6] = -1;
    relative->int_values[7] = -1;
  }

  // Get the relative year, month, day, hour, minute and second.
  if (relative->int_values[8] > 0) {
    day += RelativeDOWToDays(*relative, ts.tm_wday);
  } else {
    int multipler = (relative->int_values[0] > 0) ? 1 : -1;
    if (relative->int_values[1] > 0) {
      year += relative->int_values[1] * multipler;
    }
    if (relative->int_values[2] > 0) {
      month += relative->int_values[2] * multipler;
    }
    if (relative->int_values[3] > 0) {
      day += relative->int_values[3] * multipler;
    }
    if (relative->int_values[5] > 0) {
      hour += relative->int_values[5] * multipler;
    }
    if (relative->int_values[6] > 0) {
      minute += relative->int_values[6] * multipler;
    }
    if (relative->int_values[7] > 0) {
      second += relative->int_values[7] * multipler;
    }
  }

  if (!NormalizeDateFields(&year, &month, &day, &hour, &minute, &second)) {
    TC3_VLOG(1) << "Can not normalize date " << year << "-" << month << "-"
                << day << " " << hour << ":" << minute << ":" << second;
    return false;
  }

  // Update year, month, day, hour, minute and second of date instance. We only
  // update the time unit if the relative date has it. For example:
  // if the relative date is "1 hour ago", then we don't set minite and second
  // in data intance, but we set hour and the time unit which is larger than
  // hour like day, month and year.
  // if the relative date is "1 year ago", we only update year in date instance
  // and ignore others.
  bool set = false;
  if (relative->int_values[7] >= 0) {
    set = true;
    datetime->int_values[5] = second;
  }
  if (set || relative->int_values[6] >= 0) {
    set = true;
    datetime->int_values[4] = minute;
  }
  if (set || relative->int_values[5] >= 0) {
    set = true;
    datetime->int_values[3] = hour;
  }
  if (set || relative->int_values[3] >= 0 || relative->int_values[8] >= 0) {
    set = true;
    datetime->int_values[2] = day;
  }
  if (set || relative->int_values[2] >= 0) {
    set = true;
    datetime->int_values[1] = month;
  }
  if (set || relative->int_values[1] >= 0) {
    set = true;
    datetime->int_values[0] = year;
  }
  return true;
}

// If the year is less than 100 and has no bc/ad, it should be normalized.
static constexpr int kMinYearForNormalization = 100;

// Normalize date instance.
void NormalizeDateInstance(time_t timestamp, AnnotationData* inst) {
  struct tm ts;
  localtime_r(&timestamp, &ts);

  int idx = GetPropertyIndex(kDateTime, *inst);
  if (idx >= 0) {
    Property* datetime = &inst->properties[idx];
    int bc_ad = -1;
    idx = GetPropertyIndex(kDateTimeSupplementary, *inst);
    if (idx >= 0) {
      bc_ad = inst->properties[idx].int_values[0];
    }

    int year = datetime->int_values[0];
    if (bc_ad < 0 && year > 0 && year < kMinYearForNormalization) {
      if (2000 + year <= ts.tm_year + 1900) {
        datetime->int_values[0] = 2000 + year;
      } else {
        datetime->int_values[0] = 1900 + year;
      }
    }
    // Day-of-week never only appear in date instance, it must be in both
    // relative date and non-relative date. If the date instance already has day
    // like "Monday, March 19", it doesn't make sense to convert the dow to
    // absolute date again.
    if (datetime->int_values[7] > 0 && datetime->int_values[2] > 0) {
      return;
    }
  }
  RelativeDateToAbsoluteDate(ts, inst);
}

// Convert normalized date instance to unix time.
time_t DateInstanceToUnixTimeInternal(time_t timestamp,
                                      const AnnotationData& inst) {
  int idx = GetPropertyIndex(kDateTime, inst);
  if (idx < 0) {
    return -1;
  }
  const Property& prop = inst.properties[idx];

  struct tm ts;
  localtime_r(&timestamp, &ts);

  if (prop.int_values[0] > 0) {
    ts.tm_year = prop.int_values[0] - 1900;
  }
  if (prop.int_values[1] > 0) {
    ts.tm_mon = prop.int_values[1] - 1;
  }
  if (prop.int_values[2] > 0) {
    ts.tm_mday = prop.int_values[2];
  }
  if (prop.int_values[3] > 0) {
    ts.tm_hour = prop.int_values[3];
  }
  if (prop.int_values[4] > 0) {
    ts.tm_min = prop.int_values[4];
  }
  if (prop.int_values[5] > 0) {
    ts.tm_sec = prop.int_values[5];
  }
  ts.tm_wday = -1;
  ts.tm_yday = -1;
  return mktime(&ts);
}
}  // namespace

void NormalizeDateTimes(time_t timestamp, std::vector<Annotation>* dates) {
  for (int i = 0; i < dates->size(); ++i) {
    if ((*dates)[i].data.type == kDateTimeType) {
      NormalizeDateInstance(timestamp, &(*dates)[i].data);
    }
  }
}

namespace {
bool AnyOverlappedField(const DateMatch& prev, const DateMatch& next) {
#define Field(f) \
  if (prev.f && next.f) return true
  Field(year_match);
  Field(month_match);
  Field(day_match);
  Field(day_of_week_match);
  Field(time_value_match);
  Field(time_span_match);
  Field(time_zone_name_match);
  Field(time_zone_offset_match);
  Field(relative_match);
  Field(combined_digits_match);
#undef Field
  return false;
}

void MergeDateMatchImpl(const DateMatch& prev, DateMatch* next,
                        bool update_span) {
#define RM(f) \
  if (!next->f) next->f = prev.f
  RM(year_match);
  RM(month_match);
  RM(day_match);
  RM(day_of_week_match);
  RM(time_value_match);
  RM(time_span_match);
  RM(time_zone_name_match);
  RM(time_zone_offset_match);
  RM(relative_match);
  RM(combined_digits_match);
#undef RM

#define RV(f) \
  if (next->f == NO_VAL) next->f = prev.f
  RV(year);
  RV(month);
  RV(day);
  RV(hour);
  RV(minute);
  RV(second);
  RV(fraction_second);
#undef RV

#define RE(f, v) \
  if (next->f == v) next->f = prev.f
  RE(day_of_week, DayOfWeek_DOW_NONE);
  RE(bc_ad, BCAD_BCAD_NONE);
  RE(time_span_code, TimespanCode_TIMESPAN_CODE_NONE);
  RE(time_zone_code, TimezoneCode_TIMEZONE_CODE_NONE);
#undef RE

  if (next->time_zone_offset == std::numeric_limits<int16>::min()) {
    next->time_zone_offset = prev.time_zone_offset;
  }

  next->priority = std::max(next->priority, prev.priority);
  if (update_span) {
    next->begin = std::min(next->begin, prev.begin);
    next->end = std::max(next->end, prev.end);
  }
}
}  // namespace

bool IsDateMatchMergeable(const DateMatch& prev, const DateMatch& next) {
  // Do not merge if they share the same field.
  if (AnyOverlappedField(prev, next)) {
    return false;
  }

  // It's impossible that both prev and next have relative date since it's
  // excluded by overlapping check before.
  if (prev.HasRelativeDate() || next.HasRelativeDate()) {
    // If one of them is relative date, then we merge:
    //   - if relative match shouldn't have time, and always has DOW or day.
    //   - if not both relative match and non relative match has day.
    //   - if non relative match has time or day.
    const DateMatch* rm = &prev;
    const DateMatch* non_rm = &prev;
    if (prev.HasRelativeDate()) {
      non_rm = &next;
    } else {
      rm = &next;
    }

    const RelativeMatch* relative_match = rm->relative_match;
    // Relative Match should have day or DOW but no time.
    if (!relative_match->HasDayFields() ||
        relative_match->HasTimeValueFields()) {
      return false;
    }
    // Check if both relative match and non relative match has day.
    if (non_rm->HasDateFields() && relative_match->HasDay()) {
      return false;
    }
    // Non relative match should have either hour (time) or day (date).
    if (!non_rm->HasHour() && !non_rm->HasDay()) {
      return false;
    }
  } else {
    // Only one match has date and another has time.
    if ((prev.HasDateFields() && next.HasDateFields()) ||
        (prev.HasTimeFields() && next.HasTimeFields())) {
      return false;
    }
    // DOW never be extracted as a single DateMatch except in RelativeMatch. So
    // here, we always merge one with day and another one with hour.
    if (!(prev.HasDay() || next.HasDay()) ||
        !(prev.HasHour() || next.HasHour())) {
      return false;
    }
  }
  return true;
}

void MergeDateMatch(const DateMatch& prev, DateMatch* next, bool update_span) {
  if (IsDateMatchMergeable(prev, *next)) {
    MergeDateMatchImpl(prev, next, update_span);
  }
}

}  // namespace dates
}  // namespace libtextclassifier3
