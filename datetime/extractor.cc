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

#include "datetime/extractor.h"

#include "util/base/logging.h"

namespace libtextclassifier2 {

constexpr char const* kGroupYear = "YEAR";
constexpr char const* kGroupMonth = "MONTH";
constexpr char const* kGroupDay = "DAY";
constexpr char const* kGroupHour = "HOUR";
constexpr char const* kGroupMinute = "MINUTE";
constexpr char const* kGroupSecond = "SECOND";
constexpr char const* kGroupAmpm = "AMPM";
constexpr char const* kGroupRelationDistance = "RELATIONDISTANCE";
constexpr char const* kGroupRelation = "RELATION";
constexpr char const* kGroupRelationType = "RELATIONTYPE";
// Dummy groups serve just as an inflator of the selection. E.g. we might want
// to select more text than was contained in an envelope of all extractor spans.
constexpr char const* kGroupDummy1 = "DUMMY1";
constexpr char const* kGroupDummy2 = "DUMMY2";

bool DatetimeExtractor::Extract(DateParseData* result,
                                CodepointSpan* result_span) const {
  result->field_set_mask = 0;
  *result_span = {kInvalidIndex, kInvalidIndex};

  UnicodeText group_text;
  if (GroupNotEmpty(kGroupYear, &group_text)) {
    result->field_set_mask |= DateParseData::YEAR_FIELD;
    if (!ParseYear(group_text, &(result->year))) {
      TC_LOG(ERROR) << "Couldn't extract YEAR.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupYear, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupMonth, &group_text)) {
    result->field_set_mask |= DateParseData::MONTH_FIELD;
    if (!ParseMonth(group_text, &(result->month))) {
      TC_LOG(ERROR) << "Couldn't extract MONTH.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupMonth, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupDay, &group_text)) {
    result->field_set_mask |= DateParseData::DAY_FIELD;
    if (!ParseDigits(group_text, &(result->day_of_month))) {
      TC_LOG(ERROR) << "Couldn't extract DAY.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupDay, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupHour, &group_text)) {
    result->field_set_mask |= DateParseData::HOUR_FIELD;
    if (!ParseDigits(group_text, &(result->hour))) {
      TC_LOG(ERROR) << "Couldn't extract HOUR.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupHour, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupMinute, &group_text)) {
    result->field_set_mask |= DateParseData::MINUTE_FIELD;
    if (!ParseDigits(group_text, &(result->minute))) {
      TC_LOG(ERROR) << "Couldn't extract MINUTE.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupMinute, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupSecond, &group_text)) {
    result->field_set_mask |= DateParseData::SECOND_FIELD;
    if (!ParseDigits(group_text, &(result->second))) {
      TC_LOG(ERROR) << "Couldn't extract SECOND.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupSecond, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupAmpm, &group_text)) {
    result->field_set_mask |= DateParseData::AMPM_FIELD;
    if (!ParseAMPM(group_text, &(result->ampm))) {
      TC_LOG(ERROR) << "Couldn't extract AMPM.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupAmpm, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupRelationDistance, &group_text)) {
    result->field_set_mask |= DateParseData::RELATION_DISTANCE_FIELD;
    if (!ParseRelationDistance(group_text, &(result->relation_distance))) {
      TC_LOG(ERROR) << "Couldn't extract RELATION_DISTANCE_FIELD.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupRelationDistance, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupRelation, &group_text)) {
    result->field_set_mask |= DateParseData::RELATION_FIELD;
    if (!ParseRelation(group_text, &(result->relation))) {
      TC_LOG(ERROR) << "Couldn't extract RELATION_FIELD.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupRelation, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupRelationType, &group_text)) {
    result->field_set_mask |= DateParseData::RELATION_TYPE_FIELD;
    if (!ParseRelationType(group_text, &(result->relation_type))) {
      TC_LOG(ERROR) << "Couldn't extract RELATION_TYPE_FIELD.";
      return false;
    }
    if (!UpdateMatchSpan(kGroupRelationType, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupDummy1, &group_text)) {
    if (!UpdateMatchSpan(kGroupDummy1, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (GroupNotEmpty(kGroupDummy2, &group_text)) {
    if (!UpdateMatchSpan(kGroupDummy2, result_span)) {
      TC_LOG(ERROR) << "Couldn't update span.";
      return false;
    }
  }

  if (result_span->first == kInvalidIndex ||
      result_span->second == kInvalidIndex) {
    *result_span = {kInvalidIndex, kInvalidIndex};
  }

  return true;
}

bool DatetimeExtractor::RuleIdForType(DatetimeExtractorType type,
                                      int* rule_id) const {
  auto type_it = type_and_locale_to_rule_.find(type);
  if (type_it == type_and_locale_to_rule_.end()) {
    return false;
  }

  auto locale_it = type_it->second.find(locale_id_);
  if (locale_it == type_it->second.end()) {
    return false;
  }
  *rule_id = locale_it->second;
  return true;
}

bool DatetimeExtractor::ExtractType(const UnicodeText& input,
                                    DatetimeExtractorType extractor_type,
                                    UnicodeText* match_result) const {
  int rule_id;
  if (!RuleIdForType(extractor_type, &rule_id)) {
    return false;
  }

  std::unique_ptr<UniLib::RegexMatcher> matcher =
      rules_[rule_id]->Matcher(input);
  if (!matcher) {
    return false;
  }

  int status;
  if (!matcher->Find(&status)) {
    return false;
  }

  if (match_result != nullptr) {
    *match_result = matcher->Group(&status);
    if (status != UniLib::RegexMatcher::kNoError) {
      return false;
    }
  }
  return true;
}

bool DatetimeExtractor::GroupNotEmpty(StringPiece name,
                                      UnicodeText* result) const {
  int status;
  *result = matcher_.Group(name, &status);
  if (status != UniLib::RegexMatcher::kNoError) {
    return false;
  }
  return !result->empty();
}

bool DatetimeExtractor::UpdateMatchSpan(StringPiece name,
                                        CodepointSpan* span) const {
  int status;
  const int match_start = matcher_.Start(name, &status);
  if (status != UniLib::RegexMatcher::kNoError) {
    return false;
  }
  const int match_end = matcher_.End(name, &status);
  if (status != UniLib::RegexMatcher::kNoError) {
    return false;
  }
  if (span->first == kInvalidIndex || span->first > match_start) {
    span->first = match_start;
  }
  if (span->second == kInvalidIndex || span->second < match_end) {
    span->second = match_end;
  }

  return true;
}

template <typename T>
bool DatetimeExtractor::MapInput(
    const UnicodeText& input,
    const std::vector<std::pair<DatetimeExtractorType, T>>& mapping,
    T* result) const {
  for (const auto& type_value_pair : mapping) {
    if (ExtractType(input, type_value_pair.first)) {
      *result = type_value_pair.second;
      return true;
    }
  }
  return false;
}

bool DatetimeExtractor::ParseWrittenNumber(const UnicodeText& input,
                                           int* parsed_number) const {
  std::vector<std::pair<int, int>> found_numbers;
  for (const auto& type_value_pair :
       std::vector<std::pair<DatetimeExtractorType, int>>{
           {DatetimeExtractorType_ZERO, 0},
           {DatetimeExtractorType_ONE, 1},
           {DatetimeExtractorType_TWO, 2},
           {DatetimeExtractorType_THREE, 3},
           {DatetimeExtractorType_FOUR, 4},
           {DatetimeExtractorType_FIVE, 5},
           {DatetimeExtractorType_SIX, 6},
           {DatetimeExtractorType_SEVEN, 7},
           {DatetimeExtractorType_EIGHT, 8},
           {DatetimeExtractorType_NINE, 9},
           {DatetimeExtractorType_TEN, 10},
           {DatetimeExtractorType_ELEVEN, 11},
           {DatetimeExtractorType_TWELVE, 12},
           {DatetimeExtractorType_THIRTEEN, 13},
           {DatetimeExtractorType_FOURTEEN, 14},
           {DatetimeExtractorType_FIFTEEN, 15},
           {DatetimeExtractorType_SIXTEEN, 16},
           {DatetimeExtractorType_SEVENTEEN, 17},
           {DatetimeExtractorType_EIGHTEEN, 18},
           {DatetimeExtractorType_NINETEEN, 19},
           {DatetimeExtractorType_TWENTY, 20},
           {DatetimeExtractorType_THIRTY, 30},
           {DatetimeExtractorType_FORTY, 40},
           {DatetimeExtractorType_FIFTY, 50},
           {DatetimeExtractorType_SIXTY, 60},
           {DatetimeExtractorType_SEVENTY, 70},
           {DatetimeExtractorType_EIGHTY, 80},
           {DatetimeExtractorType_NINETY, 90},
           {DatetimeExtractorType_HUNDRED, 100},
           {DatetimeExtractorType_THOUSAND, 1000},
       }) {
    int rule_id;
    if (!RuleIdForType(type_value_pair.first, &rule_id)) {
      return false;
    }

    std::unique_ptr<UniLib::RegexMatcher> matcher =
        rules_[rule_id]->Matcher(input);
    if (!matcher) {
      return false;
    }

    int status;
    while (matcher->Find(&status) && status == UniLib::RegexMatcher::kNoError) {
      int span_start = matcher->Start(&status);
      if (status != UniLib::RegexMatcher::kNoError) {
        return false;
      }
      found_numbers.push_back({span_start, type_value_pair.second});
    }
  }

  std::sort(found_numbers.begin(), found_numbers.end(),
            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
              return a.first < b.first;
            });

  int sum = 0;
  int running_value = -1;
  // Simple math to make sure we handle written numerical modifiers correctly
  // so that :="fifty one  thousand and one" maps to 51001 and not 50 1 1000 1.
  for (const std::pair<int, int> position_number_pair : found_numbers) {
    if (running_value >= 0) {
      if (running_value > position_number_pair.second) {
        sum += running_value;
        running_value = position_number_pair.second;
      } else {
        running_value *= position_number_pair.second;
      }
    } else {
      running_value = position_number_pair.second;
    }
  }
  sum += running_value;
  *parsed_number = sum;
  return true;
}

bool DatetimeExtractor::ParseDigits(const UnicodeText& input,
                                    int* parsed_digits) const {
  UnicodeText digit;
  if (!ExtractType(input, DatetimeExtractorType_DIGITS, &digit)) {
    return false;
  }

  if (!unilib_.ParseInt32(digit, parsed_digits)) {
    return false;
  }
  return true;
}

bool DatetimeExtractor::ParseYear(const UnicodeText& input,
                                  int* parsed_year) const {
  if (!ParseDigits(input, parsed_year)) {
    return false;
  }

  if (*parsed_year < 100) {
    if (*parsed_year < 50) {
      *parsed_year += 2000;
    } else {
      *parsed_year += 1900;
    }
  }

  return true;
}

bool DatetimeExtractor::ParseMonth(const UnicodeText& input,
                                   int* parsed_month) const {
  if (ParseDigits(input, parsed_month)) {
    return true;
  }

  if (MapInput(input,
               {
                   {DatetimeExtractorType_JANUARY, 1},
                   {DatetimeExtractorType_FEBRUARY, 2},
                   {DatetimeExtractorType_MARCH, 3},
                   {DatetimeExtractorType_APRIL, 4},
                   {DatetimeExtractorType_MAY, 5},
                   {DatetimeExtractorType_JUNE, 6},
                   {DatetimeExtractorType_JULY, 7},
                   {DatetimeExtractorType_AUGUST, 8},
                   {DatetimeExtractorType_SEPTEMBER, 9},
                   {DatetimeExtractorType_OCTOBER, 10},
                   {DatetimeExtractorType_NOVEMBER, 11},
                   {DatetimeExtractorType_DECEMBER, 12},
               },
               parsed_month)) {
    return true;
  }

  return false;
}

bool DatetimeExtractor::ParseAMPM(const UnicodeText& input,
                                  int* parsed_ampm) const {
  return MapInput(input,
                  {
                      {DatetimeExtractorType_AM, DateParseData::AMPM::AM},
                      {DatetimeExtractorType_PM, DateParseData::AMPM::PM},
                  },
                  parsed_ampm);
}

bool DatetimeExtractor::ParseRelationDistance(const UnicodeText& input,
                                              int* parsed_distance) const {
  if (ParseDigits(input, parsed_distance)) {
    return true;
  }
  if (ParseWrittenNumber(input, parsed_distance)) {
    return true;
  }
  return false;
}

bool DatetimeExtractor::ParseRelation(
    const UnicodeText& input, DateParseData::Relation* parsed_relation) const {
  return MapInput(
      input,
      {
          {DatetimeExtractorType_NOW, DateParseData::Relation::NOW},
          {DatetimeExtractorType_YESTERDAY, DateParseData::Relation::YESTERDAY},
          {DatetimeExtractorType_TOMORROW, DateParseData::Relation::TOMORROW},
          {DatetimeExtractorType_NEXT, DateParseData::Relation::NEXT},
          {DatetimeExtractorType_NEXT_OR_SAME,
           DateParseData::Relation::NEXT_OR_SAME},
          {DatetimeExtractorType_LAST, DateParseData::Relation::LAST},
          {DatetimeExtractorType_PAST, DateParseData::Relation::PAST},
          {DatetimeExtractorType_FUTURE, DateParseData::Relation::FUTURE},
      },
      parsed_relation);
}

bool DatetimeExtractor::ParseRelationType(
    const UnicodeText& input,
    DateParseData::RelationType* parsed_relation_type) const {
  return MapInput(
      input,
      {
          {DatetimeExtractorType_MONDAY, DateParseData::MONDAY},
          {DatetimeExtractorType_TUESDAY, DateParseData::TUESDAY},
          {DatetimeExtractorType_WEDNESDAY, DateParseData::WEDNESDAY},
          {DatetimeExtractorType_THURSDAY, DateParseData::THURSDAY},
          {DatetimeExtractorType_FRIDAY, DateParseData::FRIDAY},
          {DatetimeExtractorType_SATURDAY, DateParseData::SATURDAY},
          {DatetimeExtractorType_SUNDAY, DateParseData::SUNDAY},
          {DatetimeExtractorType_DAY, DateParseData::DAY},
          {DatetimeExtractorType_WEEK, DateParseData::WEEK},
          {DatetimeExtractorType_MONTH, DateParseData::MONTH},
          {DatetimeExtractorType_YEAR, DateParseData::YEAR},
      },
      parsed_relation_type);
}

bool DatetimeExtractor::ParseTimeUnit(const UnicodeText& input,
                                      int* parsed_time_unit) const {
  return MapInput(input,
                  {
                      {DatetimeExtractorType_DAYS, DateParseData::DAYS},
                      {DatetimeExtractorType_WEEKS, DateParseData::WEEKS},
                      {DatetimeExtractorType_MONTHS, DateParseData::MONTHS},
                      {DatetimeExtractorType_HOURS, DateParseData::HOURS},
                      {DatetimeExtractorType_MINUTES, DateParseData::MINUTES},
                      {DatetimeExtractorType_SECONDS, DateParseData::SECONDS},
                      {DatetimeExtractorType_YEARS, DateParseData::YEARS},
                  },
                  parsed_time_unit);
}

bool DatetimeExtractor::ParseWeekday(const UnicodeText& input,
                                     int* parsed_weekday) const {
  return MapInput(
      input,
      {
          {DatetimeExtractorType_MONDAY, DateParseData::MONDAY},
          {DatetimeExtractorType_TUESDAY, DateParseData::TUESDAY},
          {DatetimeExtractorType_WEDNESDAY, DateParseData::WEDNESDAY},
          {DatetimeExtractorType_THURSDAY, DateParseData::THURSDAY},
          {DatetimeExtractorType_FRIDAY, DateParseData::FRIDAY},
          {DatetimeExtractorType_SATURDAY, DateParseData::SATURDAY},
          {DatetimeExtractorType_SUNDAY, DateParseData::SUNDAY},
      },
      parsed_weekday);
}

}  // namespace libtextclassifier2
