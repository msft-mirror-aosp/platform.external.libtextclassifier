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

#include "annotator/grammar/dates/cfg-datetime-annotator.h"

#include "annotator/grammar/dates/annotations/annotation-options.h"
#include "utils/strings/split.h"
#include "utils/tokenizer.h"
#include "utils/utf8/unicodetext.h"

static const int kAM = 0;
static const int kPM = 1;

namespace libtextclassifier3::dates {
namespace {

// Datetime annotation are stored as the repeated field of ints & the order is
// preserved as follow -year[0], -month[1], -day_of_month[2], -day[3], -week[4],
// -hour[5], -minute[6], -second[7] and day_of_week[8];
static const std::map<const DatetimeComponent::ComponentType, int>&
    kTypeToDatetimeIndex =
        *new std::map<const DatetimeComponent::ComponentType, int>{
            {DatetimeComponent::ComponentType::YEAR, 0},
            {DatetimeComponent::ComponentType::MONTH, 1},
            {DatetimeComponent::ComponentType::DAY_OF_MONTH, 2},
            {DatetimeComponent::ComponentType::HOUR, 3},
            {DatetimeComponent::ComponentType::MINUTE, 4},
            {DatetimeComponent::ComponentType::SECOND, 5},
            {DatetimeComponent::ComponentType::DAY_OF_WEEK, 7},
        };

// Datetime annotation are stored as the repeated field of ints & the order is
// preserved as follow -is_future[0], -year[1], -month[2], -day[3], -week[4],
// -hour[5], -minute[6], -second[7] and day_of_week[8];
// After first nine fields mentioned above there are more fields related to day
// of week which are used to interprete day of week. Those could be  zero or
// multiple values see RelativeEnum for details.
static const std::map<const DatetimeComponent::ComponentType, int>&
    kTypeToRelativeIndex =
        *new std::map<const DatetimeComponent::ComponentType, int>{
            {DatetimeComponent::ComponentType::YEAR, 1},
            {DatetimeComponent::ComponentType::MONTH, 2},
            {DatetimeComponent::ComponentType::DAY_OF_MONTH, 3},
            {DatetimeComponent::ComponentType::WEEK, 4},
            {DatetimeComponent::ComponentType::HOUR, 5},
            {DatetimeComponent::ComponentType::MINUTE, 6},
            {DatetimeComponent::ComponentType::SECOND, 7},
            {DatetimeComponent::ComponentType::DAY_OF_WEEK, 8},
        };

// kDateTimeSupplementary contains uncommon field like timespan, timezone. It's
// integer array and the format is (bc_ad, timespan_code, timezone_code,
// timezone_offset). Al four fields must be provided. If the field is not
// extracted, the value is -1 in the array.
static const std::map<const DatetimeComponent::ComponentType, int>&
    kDateTimeSupplementaryindex =
        *new std::map<const DatetimeComponent::ComponentType, int>{
            {DatetimeComponent::ComponentType::MERIDIEM, 1},
        };

static DatetimeComponent::RelativeQualifier GetRelativeQualifier(
    const Property& property) {
  //|| property.int_values(0) < 0
  if (property.name != kDateTimeRelative) {
    return DatetimeComponent::RelativeQualifier::UNSPECIFIED;
  }
  // Special case: there are certain scenarios in which the relative qualifier
  // is hard to determine e.g. given “Wednesday 4:00 am” it is hard to
  // determine if the event is in the past or future hence it is okay to mark
  // the relative qualifier as unspecified.
  // Given “unspecified” relative qualifier, now it is not possible to say time
  // in milliseconds which is unfortunately not optional so make it work with
  // the existing api the solution is that whenever the relative-ness of the
  // text is unspecified. The code will override it with “future”.
  if (property.int_values[0] == -1) {
    return DatetimeComponent::RelativeQualifier::FUTURE;
  }
  return property.int_values[0] > 0
             ? DatetimeComponent::RelativeQualifier::FUTURE
             : DatetimeComponent::RelativeQualifier::PAST;
}

static int GetRelativeCount(const Property& property) {
  // Relative count fields are stored from index 9 and onword.
  for (int i = 9; i < property.int_values.size(); i++) {
    switch (property.int_values[i]) {
      case RelativeParameter_::Interpretation_NEAREST_LAST:
        return -1;
      case RelativeParameter_::Interpretation_SECOND_LAST:
        return -2;
      case RelativeParameter_::Interpretation_SECOND_NEXT:
        return 2;
      case RelativeParameter_::Interpretation_PREVIOUS:
        return -1;
      case RelativeParameter_::Interpretation_COMING:
      case RelativeParameter_::Interpretation_SOME:
      case RelativeParameter_::Interpretation_NEAREST:
      case RelativeParameter_::Interpretation_NEAREST_NEXT:
        return 1;
      case RelativeParameter_::Interpretation_CURRENT:
        return 0;
    }
  }
  return 0;
}

// Resolve the  year’s ambiguity.
// If the year in the date has 4 digits i.e. DD/MM/YYYY then there is no
// ambiguity, the year value is YYYY but certain format i.e. MM/DD/YY is
// ambiguous e.g. in {April/23/15} year value can be 15 or 1915 or 2015.
// Following heuristic is used to resolve the ambiguity.
// - For YYYY there is nothing to resolve.
// - For all YY years
//    - Value less than 50 will be resolved to 20YY
//    - Value greater or equal 50 will be resolved to 19YY
static int InterpretYear(int parsed_year) {
  if (parsed_year < 100) {
    if (parsed_year < 50) {
      return parsed_year + 2000;
    }
    return parsed_year + 1900;
  }
  return parsed_year;
}

static void FillAbsoluteDatetimeComponent(
    const Property& property, DatetimeParsedData* datetime_parsed_data) {
  for (auto const& entry : kTypeToDatetimeIndex) {
    if (property.int_values[entry.second] > -1) {
      int absolute_value = property.int_values[entry.second];
      if (entry.first == DatetimeComponent::ComponentType::YEAR) {
        absolute_value = InterpretYear(absolute_value);
      }
      datetime_parsed_data->SetAbsoluteValue(entry.first, absolute_value);
    }
  }
}

static void FillRelativeDatetimeComponent(
    const Property& property, DatetimeParsedData* datetime_parsed_data) {
  for (auto const& entry : kTypeToRelativeIndex) {
    int relative_value = property.int_values[entry.second];
    if (relative_value > -1) {
      if (property.int_values.size() > 9) {
        datetime_parsed_data->SetRelativeCount(entry.first,
                                               GetRelativeCount(property));
      } else {
        datetime_parsed_data->SetRelativeCount(entry.first, relative_value);
      }
      datetime_parsed_data->SetRelativeValue(entry.first,
                                             GetRelativeQualifier(property));
    }
  }
}

static void FillSupplementaryDatetimeComponent(
    const Property& property, DatetimeParsedData* datetime_parsed_data) {
  for (auto const& entry : kDateTimeSupplementaryindex) {
    switch (property.int_values[entry.second]) {
      case TimespanCode_NOON:
        // NOON [2] -> PM
        datetime_parsed_data->SetAbsoluteValue(entry.first, kPM);
        break;
      case TimespanCode_MIDNIGHT:
        // MIDNIGHT [3] -> AM
        datetime_parsed_data->SetAbsoluteValue(entry.first, kAM);
        break;
      case TimespanCode_TONIGHT:
        // TONIGHT [11] -> PM
        datetime_parsed_data->SetAbsoluteValue(entry.first, kPM);
        break;
      case TimespanCode_AM:
      case TimespanCode_PM:
        datetime_parsed_data->SetAbsoluteValue(
            entry.first, property.int_values[entry.second]);
        break;
      case TimespanCode_TIMESPAN_CODE_NONE:
        break;
      default:
        TC3_LOG(WARNING) << "Failed to extract time span code.";
    }
  }
}

static void FillDatetimeParsedData(const Property& property,
                                   DatetimeParsedData* datetime_parsed_data) {
  // Absolute Datetime.
  if (property.name == kDateTime) {
    FillAbsoluteDatetimeComponent(property, datetime_parsed_data);
  }
  // Relative Datetime.
  if (property.name == kDateTimeRelative) {
    FillRelativeDatetimeComponent(property, datetime_parsed_data);
  }
  if (property.name == kDateTimeSupplementary) {
    FillSupplementaryDatetimeComponent(property, datetime_parsed_data);
  }
}

static std::string GetReferenceLocale(const std::string& locales) {
  std::vector<StringPiece> split_locales = strings::Split(locales, ',');
  if (!split_locales.empty()) {
    return split_locales[0].ToString();
  }
  return "";
}

static void InterpretParseData(const DatetimeParsedData& datetime_parsed_data,
                               const DateAnnotationOptions& options,
                               const CalendarLib& calendarlib,
                               int64* interpreted_time_ms_utc,
                               DatetimeGranularity* granularity) {
  DatetimeGranularity local_granularity =
      calendarlib.GetGranularity(datetime_parsed_data);
  if (!calendarlib.InterpretParseData(
          datetime_parsed_data, options.base_timestamp_millis,
          options.reference_timezone, GetReferenceLocale(options.locales),
          /*prefer_future_for_unspecified_date=*/true, interpreted_time_ms_utc,
          granularity)) {
    TC3_LOG(WARNING) << "Failed to extract time in millis and Granularity.";
    // Fallingback to DatetimeParsedData's finest granularity
    *granularity = local_granularity;
  }
}

Tokenizer BuildTokenizer(const UniLib* unilib,
                         const GrammarTokenizerOptions* options) {
  std::vector<const TokenizationCodepointRange*> codepoint_config;
  if (options->tokenization_codepoint_config() != nullptr) {
    codepoint_config.insert(codepoint_config.end(),
                            options->tokenization_codepoint_config()->begin(),
                            options->tokenization_codepoint_config()->end());
  }
  std::vector<const CodepointRange*> internal_codepoint_config;
  if (options->internal_tokenizer_codepoint_ranges() != nullptr) {
    internal_codepoint_config.insert(
        internal_codepoint_config.end(),
        options->internal_tokenizer_codepoint_ranges()->begin(),
        options->internal_tokenizer_codepoint_ranges()->end());
  }

  const bool tokenize_on_script_change =
      options->tokenization_codepoint_config() != nullptr &&
      options->tokenize_on_script_change();
  return Tokenizer(options->tokenization_type(), unilib, codepoint_config,
                   internal_codepoint_config, tokenize_on_script_change,
                   /*icu_preserve_whitespace_tokens=*/false);
}

}  // namespace

CfgDatetimeAnnotator::CfgDatetimeAnnotator(
    const UniLib& unilib, const GrammarTokenizerOptions* tokenizer_options,
    const CalendarLib& calendar_lib, const DatetimeRules* datetime_rules,
    const float annotator_target_classification_score,
    const float annotator_priority_score)
    : calendar_lib_(calendar_lib),
      tokenizer_(BuildTokenizer(&unilib, tokenizer_options)),
      parser_(unilib, datetime_rules),
      annotator_target_classification_score_(
          annotator_target_classification_score),
      annotator_priority_score_(annotator_priority_score) {}

// Helper method to convert the Thing into DatetimeParseResult.
// Thing constains the annotation instance i.e. type of the annotation and its
// properties/values
void CfgDatetimeAnnotator::FillDatetimeParseResult(
    const AnnotationData& annotation_data, const DateAnnotationOptions& options,
    DatetimeParseResult* datetime_parse_result) const {
  DatetimeParsedData datetime_parsed_data;
  for (const auto& property : annotation_data.properties) {
    FillDatetimeParsedData(property, &datetime_parsed_data);
  }
  datetime_parsed_data.GetDatetimeComponents(
      &datetime_parse_result->datetime_components);
  InterpretParseData(datetime_parsed_data, options, calendar_lib_,
                     &(datetime_parse_result->time_ms_utc),
                     &(datetime_parse_result->granularity));
  std::sort(datetime_parse_result->datetime_components.begin(),
            datetime_parse_result->datetime_components.end(),
            [](DatetimeComponent a, DatetimeComponent b) {
              return a.component_type > b.component_type;
            });
}

// Helper methods to convert the Annotation proto to collection of
// DatetimeParseResultSpan.
// DateTime Extractor extract the list of annotation from the grammar rules,
// where each annotation is a datetime span in the input string. The method will
// convert each annotation into DatetimeParseResultSpan.
void CfgDatetimeAnnotator::FillDatetimeParseResultSpan(
    const UnicodeText& unicode_text,
    const std::vector<Annotation>& annotation_list,
    const DateAnnotationOptions& options,
    std::vector<DatetimeParseResultSpan>* results) const {
  for (const Annotation& annotation : annotation_list) {
    // Annotation can be of two type 1) Datetime or 2) DatetimeRange
    if (annotation.data.type == kDateTimeType) {
      DatetimeParseResultSpan datetime_parse_result_span;
      datetime_parse_result_span.span =
          CodepointSpan{annotation.begin, annotation.end};
      datetime_parse_result_span.target_classification_score =
          annotator_target_classification_score_;
      datetime_parse_result_span.priority_score = annotator_priority_score_;

      // Though the datastructre allow multiple DatetimeParseResult per span
      // but for annotator based on grammar there will just one.
      DatetimeParseResult datetime_parse_result;
      FillDatetimeParseResult(annotation.data, options, &datetime_parse_result);
      datetime_parse_result_span.data.push_back(datetime_parse_result);
      results->push_back(datetime_parse_result_span);
    }
    // Handling of DatetimeRange is little convoluted as it contains at least
    // two instance of Datetime
    if (annotation.data.type == kDateTimeRangeType) {
      TC3_LOG(ERROR) << "Grammar based Duration/Datetime Range not implemented";
    }
  }
}

void CfgDatetimeAnnotator::Parse(
    const std::string& input, const int64 reference_time_ms_utc,
    const std::string& reference_timezone, const std::vector<Locale>& locales,
    std::vector<DatetimeParseResultSpan>* results) const {
  Parse(UTF8ToUnicodeText(input, /*do_copy=*/false), reference_time_ms_utc,
        reference_timezone, locales, results);
}

void CfgDatetimeAnnotator::Parse(
    const UnicodeText& input, const int64 reference_time_ms_utc,
    const std::string& reference_timezone, const std::vector<Locale>& locales,
    std::vector<DatetimeParseResultSpan>* results) const {
  DateAnnotationOptions annotation_options;
  annotation_options.reference_timezone = reference_timezone;
  FillDatetimeParseResultSpan(
      input,
      parser_.Parse(input.data(), tokenizer_.Tokenize(input), locales,
                    annotation_options),
      annotation_options, results);
}

}  // namespace libtextclassifier3::dates
