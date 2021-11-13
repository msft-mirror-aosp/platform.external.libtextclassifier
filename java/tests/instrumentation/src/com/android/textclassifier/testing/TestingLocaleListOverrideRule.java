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

package com.android.textclassifier.testing;

import android.app.UiAutomation;
import android.os.LocaleList;
import android.util.Log;
import androidx.test.platform.app.InstrumentationRegistry;
import org.junit.rules.ExternalResource;

/** class for overriding testing_locale_list_override from {@link TextClassifierSettings} */
public final class TestingLocaleListOverrideRule extends ExternalResource {
  private static final String TAG = "TestingLocaleListOverrideRule";

  private LocaleList originalLocaleList;

  @Override
  protected void before() {
    originalLocaleList = LocaleList.getDefault();
  }

  public void set(String... localeTags) {
    if (localeTags.length == 0) {
      return;
    }
    runShellCommand(
        "device_config put textclassifier testing_locale_list_override "
            + String.join(",", localeTags));
  }

  @Override
  protected void after() {
    runShellCommand(
        "device_config put textclassifier testing_locale_list_override "
            + originalLocaleList.toLanguageTags());
    runShellCommand("device_config delete textclassifier testing_locale_list_override");
  }

  private static void runShellCommand(String cmd) {
    Log.v(TAG, "run shell command: " + cmd);
    UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
    uiAutomation.executeShellCommand(cmd);
  }
}
