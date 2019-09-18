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

#include "utils/utf8/unilib-javaicu.h"

#include <cassert>
#include <cctype>
#include <map>

#include "utils/java/string_utils.h"
#include "utils/utf8/unilib-common.h"

namespace libtextclassifier3 {

UniLib::UniLib() {
  TC3_LOG(FATAL) << "Java ICU UniLib must be initialized with a JniCache.";
}

UniLib::UniLib(const std::shared_ptr<JniCache>& jni_cache)
    : jni_cache_(jni_cache) {}

bool UniLib::IsOpeningBracket(char32 codepoint) const {
  return libtextclassifier3::IsOpeningBracket(codepoint);
}

bool UniLib::IsClosingBracket(char32 codepoint) const {
  return libtextclassifier3::IsClosingBracket(codepoint);
}

bool UniLib::IsWhitespace(char32 codepoint) const {
  return libtextclassifier3::IsWhitespace(codepoint);
}

bool UniLib::IsDigit(char32 codepoint) const {
  return libtextclassifier3::IsDigit(codepoint);
}

bool UniLib::IsLower(char32 codepoint) const {
  return libtextclassifier3::IsLower(codepoint);
}

bool UniLib::IsUpper(char32 codepoint) const {
  return libtextclassifier3::IsUpper(codepoint);
}

char32 UniLib::ToLower(char32 codepoint) const {
  return libtextclassifier3::ToLower(codepoint);
}

char32 UniLib::ToUpper(char32 codepoint) const {
  return libtextclassifier3::ToUpper(codepoint);
}

char32 UniLib::GetPairedBracket(char32 codepoint) const {
  return libtextclassifier3::GetPairedBracket(codepoint);
}

// -----------------------------------------------------------------------------
// Implementations that call out to JVM. Behold the beauty.
// -----------------------------------------------------------------------------

bool UniLib::ParseInt32(const UnicodeText& text, int* result) const {
  if (jni_cache_) {
    JNIEnv* env = jni_cache_->GetEnv();
    const ScopedLocalRef<jstring> text_java =
        jni_cache_->ConvertToJavaString(text);
    jint res = env->CallStaticIntMethod(jni_cache_->integer_class.get(),
                                        jni_cache_->integer_parse_int,
                                        text_java.get());
    if (jni_cache_->ExceptionCheckAndClear()) {
      return false;
    }
    *result = res;
    return true;
  }
  return false;
}

std::unique_ptr<UniLib::RegexPattern> UniLib::CreateRegexPattern(
    const UnicodeText& regex) const {
  return std::unique_ptr<UniLib::RegexPattern>(
      new UniLib::RegexPattern(jni_cache_.get(), regex, /*lazy=*/false));
}

std::unique_ptr<UniLib::RegexPattern> UniLib::CreateLazyRegexPattern(
    const UnicodeText& regex) const {
  return std::unique_ptr<UniLib::RegexPattern>(
      new UniLib::RegexPattern(jni_cache_.get(), regex, /*lazy=*/true));
}

UniLib::RegexPattern::RegexPattern(const JniCache* jni_cache,
                                   const UnicodeText& pattern, bool lazy)
    : jni_cache_(jni_cache),
      pattern_(nullptr, jni_cache ? jni_cache->jvm : nullptr),
      initialized_(false),
      initialization_failure_(false),
      pattern_text_(pattern) {
  if (!lazy) {
    LockedInitializeIfNotAlready();
  }
}

void UniLib::RegexPattern::LockedInitializeIfNotAlready() const {
  std::lock_guard<std::mutex> guard(mutex_);
  if (initialized_ || initialization_failure_) {
    return;
  }

  if (jni_cache_) {
    JNIEnv* jenv = jni_cache_->GetEnv();
    const ScopedLocalRef<jstring> regex_java =
        jni_cache_->ConvertToJavaString(pattern_text_);
    pattern_ = MakeGlobalRef(jenv->CallStaticObjectMethod(
                                 jni_cache_->pattern_class.get(),
                                 jni_cache_->pattern_compile, regex_java.get()),
                             jenv, jni_cache_->jvm);

    if (jni_cache_->ExceptionCheckAndClear() || pattern_ == nullptr) {
      initialization_failure_ = true;
      pattern_.reset();
      return;
    }

    initialized_ = true;
    pattern_text_.clear();  // We don't need this anymore.
  }
}

constexpr int UniLib::RegexMatcher::kError;
constexpr int UniLib::RegexMatcher::kNoError;

std::unique_ptr<UniLib::RegexMatcher> UniLib::RegexPattern::Matcher(
    const UnicodeText& context) const {
  LockedInitializeIfNotAlready();  // Possibly lazy initialization.
  if (initialization_failure_) {
    return nullptr;
  }

  if (jni_cache_) {
    JNIEnv* env = jni_cache_->GetEnv();
    const jstring context_java =
        jni_cache_->ConvertToJavaString(context).release();
    if (!context_java) {
      return nullptr;
    }
    const jobject matcher = env->CallObjectMethod(
        pattern_.get(), jni_cache_->pattern_matcher, context_java);
    if (jni_cache_->ExceptionCheckAndClear() || !matcher) {
      return nullptr;
    }
    return std::unique_ptr<UniLib::RegexMatcher>(new RegexMatcher(
        jni_cache_, MakeGlobalRef(matcher, env, jni_cache_->jvm),
        MakeGlobalRef(context_java, env, jni_cache_->jvm)));
  } else {
    // NOTE: A valid object needs to be created here to pass the interface
    // tests.
    return std::unique_ptr<UniLib::RegexMatcher>(
        new RegexMatcher(jni_cache_, nullptr, nullptr));
  }
}

UniLib::RegexMatcher::RegexMatcher(const JniCache* jni_cache,
                                   ScopedGlobalRef<jobject> matcher,
                                   ScopedGlobalRef<jstring> text)
    : jni_cache_(jni_cache),
      matcher_(std::move(matcher)),
      text_(std::move(text)) {}

bool UniLib::RegexMatcher::Matches(int* status) const {
  if (jni_cache_) {
    *status = kNoError;
    const bool result = jni_cache_->GetEnv()->CallBooleanMethod(
        matcher_.get(), jni_cache_->matcher_matches);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return false;
    }
    return result;
  } else {
    *status = kError;
    return false;
  }
}

bool UniLib::RegexMatcher::ApproximatelyMatches(int* status) {
  *status = kNoError;

  jni_cache_->GetEnv()->CallObjectMethod(matcher_.get(),
                                         jni_cache_->matcher_reset);
  if (jni_cache_->ExceptionCheckAndClear()) {
    *status = kError;
    return kError;
  }

  if (!Find(status) || *status != kNoError) {
    return false;
  }

  const int found_start = jni_cache_->GetEnv()->CallIntMethod(
      matcher_.get(), jni_cache_->matcher_start_idx, 0);
  if (jni_cache_->ExceptionCheckAndClear()) {
    *status = kError;
    return kError;
  }

  const int found_end = jni_cache_->GetEnv()->CallIntMethod(
      matcher_.get(), jni_cache_->matcher_end_idx, 0);
  if (jni_cache_->ExceptionCheckAndClear()) {
    *status = kError;
    return kError;
  }

  int context_length_bmp = jni_cache_->GetEnv()->CallIntMethod(
      text_.get(), jni_cache_->string_length);
  if (jni_cache_->ExceptionCheckAndClear()) {
    *status = kError;
    return false;
  }

  if (found_start != 0 || found_end != context_length_bmp) {
    return false;
  }

  return true;
}

bool UniLib::RegexMatcher::UpdateLastFindOffset() const {
  if (!last_find_offset_dirty_) {
    return true;
  }

  const int find_offset = jni_cache_->GetEnv()->CallIntMethod(
      matcher_.get(), jni_cache_->matcher_start_idx, 0);
  if (jni_cache_->ExceptionCheckAndClear()) {
    return false;
  }

  const int codepoint_count = jni_cache_->GetEnv()->CallIntMethod(
      text_.get(), jni_cache_->string_code_point_count, last_find_offset_,
      find_offset);
  if (jni_cache_->ExceptionCheckAndClear()) {
    return false;
  }

  last_find_offset_codepoints_ += codepoint_count;
  last_find_offset_ = find_offset;
  last_find_offset_dirty_ = false;

  return true;
}

bool UniLib::RegexMatcher::Find(int* status) {
  if (jni_cache_) {
    const bool result = jni_cache_->GetEnv()->CallBooleanMethod(
        matcher_.get(), jni_cache_->matcher_find);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return false;
    }

    last_find_offset_dirty_ = true;
    *status = kNoError;
    return result;
  } else {
    *status = kError;
    return false;
  }
}

int UniLib::RegexMatcher::Start(int* status) const {
  return Start(/*group_idx=*/0, status);
}

int UniLib::RegexMatcher::Start(int group_idx, int* status) const {
  if (jni_cache_) {
    *status = kNoError;

    if (!UpdateLastFindOffset()) {
      *status = kError;
      return kError;
    }

    const int java_index = jni_cache_->GetEnv()->CallIntMethod(
        matcher_.get(), jni_cache_->matcher_start_idx, group_idx);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return kError;
    }

    // If the group didn't participate in the match the index is -1.
    if (java_index == -1) {
      return -1;
    }

    const int unicode_index = jni_cache_->GetEnv()->CallIntMethod(
        text_.get(), jni_cache_->string_code_point_count, last_find_offset_,
        java_index);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return kError;
    }

    return unicode_index + last_find_offset_codepoints_;
  } else {
    *status = kError;
    return kError;
  }
}

int UniLib::RegexMatcher::End(int* status) const {
  return End(/*group_idx=*/0, status);
}

int UniLib::RegexMatcher::End(int group_idx, int* status) const {
  if (jni_cache_) {
    *status = kNoError;

    if (!UpdateLastFindOffset()) {
      *status = kError;
      return kError;
    }

    const int java_index = jni_cache_->GetEnv()->CallIntMethod(
        matcher_.get(), jni_cache_->matcher_end_idx, group_idx);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return kError;
    }

    // If the group didn't participate in the match the index is -1.
    if (java_index == -1) {
      return -1;
    }

    const int unicode_index = jni_cache_->GetEnv()->CallIntMethod(
        text_.get(), jni_cache_->string_code_point_count, last_find_offset_,
        java_index);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return kError;
    }

    return unicode_index + last_find_offset_codepoints_;
  } else {
    *status = kError;
    return kError;
  }
}

UnicodeText UniLib::RegexMatcher::Group(int* status) const {
  if (jni_cache_) {
    JNIEnv* jenv = jni_cache_->GetEnv();
    const ScopedLocalRef<jstring> java_result(
        reinterpret_cast<jstring>(
            jenv->CallObjectMethod(matcher_.get(), jni_cache_->matcher_group)),
        jenv);
    if (jni_cache_->ExceptionCheckAndClear() || !java_result) {
      *status = kError;
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }

    std::string result;
    if (!JStringToUtf8String(jenv, java_result.get(), &result)) {
      *status = kError;
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }
    *status = kNoError;
    return UTF8ToUnicodeText(result, /*do_copy=*/true);
  } else {
    *status = kError;
    return UTF8ToUnicodeText("", /*do_copy=*/false);
  }
}

UnicodeText UniLib::RegexMatcher::Group(int group_idx, int* status) const {
  if (jni_cache_) {
    JNIEnv* jenv = jni_cache_->GetEnv();
    const ScopedLocalRef<jstring> java_result(
        reinterpret_cast<jstring>(jenv->CallObjectMethod(
            matcher_.get(), jni_cache_->matcher_group_idx, group_idx)),
        jenv);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      TC3_LOG(ERROR) << "Exception occurred";
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }

    // java_result is nullptr when the group did not participate in the match.
    // For these cases other UniLib implementations return empty string, and
    // the participation can be checked by checking if Start() == -1.
    if (!java_result) {
      *status = kNoError;
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }

    std::string result;
    if (!JStringToUtf8String(jenv, java_result.get(), &result)) {
      *status = kError;
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }
    *status = kNoError;
    return UTF8ToUnicodeText(result, /*do_copy=*/true);
  } else {
    *status = kError;
    return UTF8ToUnicodeText("", /*do_copy=*/false);
  }
}

constexpr int UniLib::BreakIterator::kDone;

UniLib::BreakIterator::BreakIterator(const JniCache* jni_cache,
                                     const UnicodeText& text)
    : jni_cache_(jni_cache),
      text_(nullptr, jni_cache ? jni_cache->jvm : nullptr),
      iterator_(nullptr, jni_cache ? jni_cache->jvm : nullptr),
      last_break_index_(0),
      last_unicode_index_(0) {
  if (jni_cache_) {
    JNIEnv* jenv = jni_cache_->GetEnv();
    text_ = MakeGlobalRef(jni_cache_->ConvertToJavaString(text).release(), jenv,
                          jni_cache->jvm);
    if (!text_) {
      return;
    }

    iterator_ = MakeGlobalRef(
        jenv->CallStaticObjectMethod(jni_cache->breakiterator_class.get(),
                                     jni_cache->breakiterator_getwordinstance,
                                     jni_cache->locale_us.get()),
        jenv, jni_cache->jvm);
    if (!iterator_) {
      return;
    }
    jenv->CallVoidMethod(iterator_.get(), jni_cache->breakiterator_settext,
                         text_.get());
  }
}

int UniLib::BreakIterator::Next() {
  if (jni_cache_) {
    const int break_index = jni_cache_->GetEnv()->CallIntMethod(
        iterator_.get(), jni_cache_->breakiterator_next);
    if (jni_cache_->ExceptionCheckAndClear() ||
        break_index == BreakIterator::kDone) {
      return BreakIterator::kDone;
    }

    const int token_unicode_length = jni_cache_->GetEnv()->CallIntMethod(
        text_.get(), jni_cache_->string_code_point_count, last_break_index_,
        break_index);
    if (jni_cache_->ExceptionCheckAndClear()) {
      return BreakIterator::kDone;
    }

    last_break_index_ = break_index;
    return last_unicode_index_ += token_unicode_length;
  }
  return BreakIterator::kDone;
}

std::unique_ptr<UniLib::BreakIterator> UniLib::CreateBreakIterator(
    const UnicodeText& text) const {
  return std::unique_ptr<UniLib::BreakIterator>(
      new UniLib::BreakIterator(jni_cache_.get(), text));
}

}  // namespace libtextclassifier3
