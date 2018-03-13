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

#include "datetime/parser.h"

#include <set>
#include <unordered_set>

#include "datetime/extractor.h"
#include "util/calendar/calendar.h"
#include "util/strings/split.h"

namespace libtextclassifier2 {
std::unique_ptr<DatetimeParser> DatetimeParser::Instance(
    const DatetimeModel* model, const UniLib& unilib) {
  std::unique_ptr<DatetimeParser> result(new DatetimeParser(model, unilib));
  if (!result->initialized_) {
    result.reset();
  }
  return result;
}

DatetimeParser::DatetimeParser(const DatetimeModel* model, const UniLib& unilib)
    : unilib_(unilib) {
  initialized_ = false;
  for (int i = 0; i < model->patterns()->Length(); ++i) {
    const DatetimeModelPattern* pattern = model->patterns()->Get(i);
    for (int j = 0; j < pattern->regexes()->Length(); ++j) {
      std::unique_ptr<UniLib::RegexPattern> regex_pattern =
          unilib.CreateRegexPattern(UTF8ToUnicodeText(
              pattern->regexes()->Get(j)->str(), /*do_copy=*/false));
      if (!regex_pattern) {
        TC_LOG(ERROR) << "Couldn't create pattern: "
                      << pattern->regexes()->Get(j)->str();
        return;
      }
      rules_.push_back(std::move(regex_pattern));
      rule_id_to_pattern_.push_back(pattern);
      for (int k = 0; k < pattern->locales()->Length(); ++k) {
        locale_to_rules_[pattern->locales()->Get(k)].push_back(rules_.size() -
                                                               1);
      }
    }
  }

  for (int i = 0; i < model->extractors()->Length(); ++i) {
    const DatetimeModelExtractor* extractor = model->extractors()->Get(i);
    std::unique_ptr<UniLib::RegexPattern> regex_pattern =
        unilib.CreateRegexPattern(
            UTF8ToUnicodeText(extractor->pattern()->str(), /*do_copy=*/false));
    if (!regex_pattern) {
      TC_LOG(ERROR) << "Couldn't create pattern: "
                    << extractor->pattern()->str();
      return;
    }
    extractor_rules_.push_back(std::move(regex_pattern));

    for (int j = 0; j < extractor->locales()->Length(); ++j) {
      type_and_locale_to_extractor_rule_[extractor->extractor()]
                                        [extractor->locales()->Get(j)] = i;
    }
  }

  for (int i = 0; i < model->locales()->Length(); ++i) {
    locale_string_to_id_[model->locales()->Get(i)->str()] = i;
  }

  use_extractors_for_locating_ = model->use_extractors_for_locating();

  initialized_ = true;
}

bool DatetimeParser::Parse(
    const std::string& input, const int64 reference_time_ms_utc,
    const std::string& reference_timezone, const std::string& locales,
    ModeFlag mode, std::vector<DatetimeParseResultSpan>* results) const {
  return Parse(UTF8ToUnicodeText(input, /*do_copy=*/false),
               reference_time_ms_utc, reference_timezone, locales, mode,
               results);
}

bool DatetimeParser::Parse(
    const UnicodeText& input, const int64 reference_time_ms_utc,
    const std::string& reference_timezone, const std::string& locales,
    ModeFlag mode, std::vector<DatetimeParseResultSpan>* results) const {
  std::vector<DatetimeParseResultSpan> found_spans;
  std::unordered_set<int> executed_rules;
  for (const int locale_id : ParseLocales(locales)) {
    auto rules_it = locale_to_rules_.find(locale_id);
    if (rules_it == locale_to_rules_.end()) {
      continue;
    }

    for (const int rule_id : rules_it->second) {
      // Skip rules that were already executed in previous locales.
      if (executed_rules.find(rule_id) != executed_rules.end()) {
        continue;
      }

      if (!(rule_id_to_pattern_[rule_id]->enabled_modes() & mode)) {
        continue;
      }

      executed_rules.insert(rule_id);

      if (!ParseWithRule(*rules_[rule_id], rule_id_to_pattern_[rule_id], input,
                         reference_time_ms_utc, reference_timezone, locale_id,
                         &found_spans)) {
        return false;
      }
    }
  }

  // Resolve conflicts by always picking the longer span.
  std::sort(
      found_spans.begin(), found_spans.end(),
      [](const DatetimeParseResultSpan& a, const DatetimeParseResultSpan& b) {
        return (a.span.second - a.span.first) > (b.span.second - b.span.first);
      });

  std::set<int, std::function<bool(int, int)>> chosen_indices_set(
      [&found_spans](int a, int b) {
        return found_spans[a].span.first < found_spans[b].span.first;
      });
  for (int i = 0; i < found_spans.size(); ++i) {
    if (!DoesCandidateConflict(i, found_spans, chosen_indices_set)) {
      chosen_indices_set.insert(i);
      results->push_back(found_spans[i]);
    }
  }

  return true;
}

bool DatetimeParser::ParseWithRule(
    const UniLib::RegexPattern& regex, const DatetimeModelPattern* pattern,
    const UnicodeText& input, const int64 reference_time_ms_utc,
    const std::string& reference_timezone, const int locale_id,
    std::vector<DatetimeParseResultSpan>* result) const {
  std::unique_ptr<UniLib::RegexMatcher> matcher = regex.Matcher(input);

  int status = UniLib::RegexMatcher::kNoError;
  while (matcher->Find(&status) && status == UniLib::RegexMatcher::kNoError) {
    const int start = matcher->Start(&status);
    if (status != UniLib::RegexMatcher::kNoError) {
      return false;
    }

    const int end = matcher->End(&status);
    if (status != UniLib::RegexMatcher::kNoError) {
      return false;
    }

    DatetimeParseResultSpan parse_result;
    if (!ExtractDatetime(*matcher, reference_time_ms_utc, reference_timezone,
                         locale_id, &(parse_result.data), &parse_result.span)) {
      return false;
    }
    if (!use_extractors_for_locating_) {
      parse_result.span = {start, end};
    }
    parse_result.target_classification_score =
        pattern->target_classification_score();
    parse_result.priority_score = pattern->priority_score();

    result->push_back(parse_result);
  }
  return true;
}

constexpr char const* kDefaultLocale = "";

std::vector<int> DatetimeParser::ParseLocales(
    const std::string& locales) const {
  std::vector<std::string> split_locales = strings::Split(locales, ',');

  // Add a default fallback locale to the end of the list.
  split_locales.push_back(kDefaultLocale);

  std::vector<int> result;
  for (const std::string& locale : split_locales) {
    auto locale_it = locale_string_to_id_.find(locale);
    if (locale_it == locale_string_to_id_.end()) {
      TC_LOG(INFO) << "Ignoring locale: " << locale;
      continue;
    }
    result.push_back(locale_it->second);
  }
  return result;
}

namespace {

DatetimeGranularity GetGranularity(const DateParseData& data) {
  DatetimeGranularity granularity = DatetimeGranularity::GRANULARITY_YEAR;
  if ((data.field_set_mask & DateParseData::YEAR_FIELD) ||
      (data.field_set_mask & DateParseData::RELATION_TYPE_FIELD &&
       (data.relation_type == DateParseData::RelationType::YEAR))) {
    granularity = DatetimeGranularity::GRANULARITY_YEAR;
  }
  if ((data.field_set_mask & DateParseData::MONTH_FIELD) ||
      (data.field_set_mask & DateParseData::RELATION_TYPE_FIELD &&
       (data.relation_type == DateParseData::RelationType::MONTH))) {
    granularity = DatetimeGranularity::GRANULARITY_MONTH;
  }
  if (data.field_set_mask & DateParseData::RELATION_TYPE_FIELD &&
      (data.relation_type == DateParseData::RelationType::WEEK)) {
    granularity = DatetimeGranularity::GRANULARITY_WEEK;
  }
  if (data.field_set_mask & DateParseData::DAY_FIELD ||
      (data.field_set_mask & DateParseData::RELATION_FIELD &&
       (data.relation == DateParseData::Relation::NOW ||
        data.relation == DateParseData::Relation::TOMORROW ||
        data.relation == DateParseData::Relation::YESTERDAY)) ||
      (data.field_set_mask & DateParseData::RELATION_TYPE_FIELD &&
       (data.relation_type == DateParseData::RelationType::MONDAY ||
        data.relation_type == DateParseData::RelationType::TUESDAY ||
        data.relation_type == DateParseData::RelationType::WEDNESDAY ||
        data.relation_type == DateParseData::RelationType::THURSDAY ||
        data.relation_type == DateParseData::RelationType::FRIDAY ||
        data.relation_type == DateParseData::RelationType::SATURDAY ||
        data.relation_type == DateParseData::RelationType::SUNDAY ||
        data.relation_type == DateParseData::RelationType::DAY))) {
    granularity = DatetimeGranularity::GRANULARITY_DAY;
  }
  if (data.field_set_mask & DateParseData::HOUR_FIELD) {
    granularity = DatetimeGranularity::GRANULARITY_HOUR;
  }
  if (data.field_set_mask & DateParseData::MINUTE_FIELD) {
    granularity = DatetimeGranularity::GRANULARITY_MINUTE;
  }
  if (data.field_set_mask & DateParseData::SECOND_FIELD) {
    granularity = DatetimeGranularity::GRANULARITY_SECOND;
  }
  return granularity;
}

}  // namespace

bool DatetimeParser::ExtractDatetime(const UniLib::RegexMatcher& matcher,
                                     const int64 reference_time_ms_utc,
                                     const std::string& reference_timezone,
                                     int locale_id, DatetimeParseResult* result,
                                     CodepointSpan* result_span) const {
  DateParseData parse;
  DatetimeExtractor extractor(matcher, locale_id, unilib_, extractor_rules_,
                              type_and_locale_to_extractor_rule_);
  if (!extractor.Extract(&parse, result_span)) {
    return false;
  }

  if (!calendar_lib_.InterpretParseData(parse, reference_time_ms_utc,
                                        reference_timezone,
                                        &(result->time_ms_utc))) {
    return false;
  }
  result->granularity = GetGranularity(parse);

  return true;
}

}  // namespace libtextclassifier2
