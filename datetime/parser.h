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

#ifndef LIBTEXTCLASSIFIER_DATETIME_PARSER_H_
#define LIBTEXTCLASSIFIER_DATETIME_PARSER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "model_generated.h"
#include "types.h"
#include "util/base/integral_types.h"
#include "util/calendar/calendar.h"
#include "util/utf8/unilib.h"

namespace libtextclassifier2 {

// Parses datetime expressions in the input and resolves them to actual absolute
// time.
class DatetimeParser {
 public:
  static std::unique_ptr<DatetimeParser> Instance(const DatetimeModel* model,
                                                  const UniLib& unilib);

  // Parses the dates in 'input' and fills result. Makes sure that the results
  // do not overlap.
  bool Parse(const std::string& input, int64 reference_time_ms_utc,
             const std::string& reference_timezone, const std::string& locales,
             ModeFlag mode,
             std::vector<DatetimeParseResultSpan>* results) const;

  // Same as above but takes UnicodeText.
  bool Parse(const UnicodeText& input, int64 reference_time_ms_utc,
             const std::string& reference_timezone, const std::string& locales,
             ModeFlag mode,
             std::vector<DatetimeParseResultSpan>* results) const;

 protected:
  DatetimeParser(const DatetimeModel* model, const UniLib& unilib);

  // Returns a list of locale ids for given locale spec string (comma-separated
  // locale names).
  std::vector<int> ParseLocales(const std::string& locales) const;
  bool ParseWithRule(const UniLib::RegexPattern& regex,
                     const DatetimeModelPattern* pattern,
                     const UnicodeText& input, int64 reference_time_ms_utc,
                     const std::string& reference_timezone, const int locale_id,
                     std::vector<DatetimeParseResultSpan>* result) const;

  // Converts the current match in 'matcher' into DatetimeParseResult.
  bool ExtractDatetime(const UniLib::RegexMatcher& matcher,
                       int64 reference_time_ms_utc,
                       const std::string& reference_timezone, int locale_id,
                       DatetimeParseResult* result,
                       CodepointSpan* result_span) const;

 private:
  bool initialized_;
  const UniLib& unilib_;
  std::vector<const DatetimeModelPattern*> rule_id_to_pattern_;
  std::vector<std::unique_ptr<const UniLib::RegexPattern>> rules_;
  std::unordered_map<int, std::vector<int>> locale_to_rules_;
  std::vector<std::unique_ptr<const UniLib::RegexPattern>> extractor_rules_;
  std::unordered_map<DatetimeExtractorType, std::unordered_map<int, int>>
      type_and_locale_to_extractor_rule_;
  std::unordered_map<std::string, int> locale_string_to_id_;
  CalendarLib calendar_lib_;
  bool use_extractors_for_locating_;
};

}  // namespace libtextclassifier2

#endif  // LIBTEXTCLASSIFIER_DATETIME_PARSER_H_
