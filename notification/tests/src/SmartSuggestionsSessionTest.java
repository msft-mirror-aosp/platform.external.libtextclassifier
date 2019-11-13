/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.textclassifier.notification;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;
import com.google.common.collect.ImmutableMap;
import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class SmartSuggestionsSessionTest {

  @Test
  public void setSeenEventLogged() {
    SmartSuggestionsSession session = new SmartSuggestionsSession("resultId", ImmutableMap.of());
    assertThat(session.isSeenEventLogged()).isFalse();
    session.setSeenEventLogged();
    assertThat(session.isSeenEventLogged()).isTrue();
  }

  @Test
  public void getResultId() {
    SmartSuggestionsSession session = new SmartSuggestionsSession("resultId", ImmutableMap.of());

    assertThat(session.getResultId()).isEqualTo("resultId");
  }

  @Test
  public void getRepliesScores() {
    SmartSuggestionsSession session =
        new SmartSuggestionsSession("resultId", ImmutableMap.of("reply", 0.5f));

    assertThat(session.getRepliesScores()).containsExactly("reply", 0.5f);
  }
}
