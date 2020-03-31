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

#include "utils/grammar/callback-delegate.h"

#include "utils/grammar/matcher.h"

namespace libtextclassifier3::grammar {

void CallbackDelegate::MatchFound(const Match* match,
                                  const CallbackId callback_id,
                                  const int64 callback_param,
                                  Matcher* matcher) {
  switch (static_cast<DefaultCallback>(callback_id)) {
    case DefaultCallback::kSetType: {
      HandleTypedMatch(match, /*type=*/callback_param, matcher);
      break;
    }
    case DefaultCallback::kAssertion: {
      HandleAssertion(match, /*negative=*/(callback_param != 0), matcher);
      break;
    }
    case DefaultCallback::kMapping: {
      HandleMapping(match, /*value=*/callback_param, matcher);
      break;
    }
    default:
      break;
  }
}

void CallbackDelegate::HandleTypedMatch(const Match* match, const int16 type,
                                        Matcher* matcher) const {
  // Allocate match on matcher arena and initialize.
  // Will be deallocated by the arena.
  Match* typed_match = matcher->AllocateAndInitMatch<Match>(*match);
  typed_match->type = type;

  // Queue the match.
  matcher->AddMatch(typed_match);
}

void CallbackDelegate::HandleAssertion(const grammar::Match* match,
                                       const bool negative,
                                       Matcher* matcher) const {
  // Allocate match on matcher arena and initialize.
  // Will be deallocated by the arena.
  AssertionMatch* assertion_match =
      matcher->AllocateAndInitMatch<AssertionMatch>(*match);
  assertion_match->type = Match::kAssertionMatch;
  assertion_match->negative = negative;

  // Queue the match.
  matcher->AddMatch(assertion_match);
}

void CallbackDelegate::HandleMapping(const Match* match, int64 value,
                                     Matcher* matcher) const {
  // Allocate match on matcher arena and initialize.
  // Will be deallocated by the arena.
  MappingMatch* mapping_match =
      matcher->AllocateAndInitMatch<MappingMatch>(*match);
  mapping_match->type = Match::kMappingMatch;
  mapping_match->id = value;

  // Queue the match.
  matcher->AddMatch(mapping_match);
}

}  // namespace libtextclassifier3::grammar
