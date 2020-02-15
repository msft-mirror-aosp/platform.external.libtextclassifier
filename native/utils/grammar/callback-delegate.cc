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

void CallbackDelegate::HandleCapturingMatch(const Match* match,
                                            const uint64 match_id,
                                            Matcher* matcher) const {
  // Allocate match on matcher arena and initialize.
  // Will be deallocated by the arena.
  CapturingMatch* capturing_match =
      matcher->AllocateAndInitMatch<CapturingMatch>(*match);
  capturing_match->type = Match::kCapturingMatch;
  capturing_match->id = static_cast<uint16>(match_id);

  // Queue the match.
  matcher->AddMatch(capturing_match);
}

void CallbackDelegate::HandleAssertion(const grammar::Match* match,
                                       bool negative, Matcher* matcher) const {
  // Allocate match on matcher arena and initialize.
  // Will be deallocated by the arena.
  AssertionMatch* assertion_match =
      matcher->AllocateAndInitMatch<AssertionMatch>(*match);
  assertion_match->type = Match::kAssertionMatch;
  assertion_match->negative = negative;

  // Queue the match.
  matcher->AddMatch(assertion_match);
}

}  // namespace libtextclassifier3::grammar
