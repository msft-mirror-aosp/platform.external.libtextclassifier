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

package com.android.textclassifier.ulp;

import static com.google.common.truth.Truth.assertThat;

import android.content.res.Resources;
import android.os.LocaleList;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import java.util.List;
import java.util.Locale;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class SystemLanguagesProviderTest {
  private SystemLanguagesProvider mSystemLanguagesProvider;

  @Before
  public void setup() {
    mSystemLanguagesProvider = new SystemLanguagesProvider();
  }

  @Test
  public void getSystemLanguageTags_singleLanguages() {
    Resources.getSystem().getConfiguration().setLocales(new LocaleList(Locale.FRANCE));

    List<String> systemLanguageTags = mSystemLanguagesProvider.getSystemLanguageTags();

    assertThat(systemLanguageTags).containsExactly("fr");
  }

  @Test
  public void getSystemLanguageTags_multipleLanguages() {
    Resources.getSystem()
        .getConfiguration()
        .setLocales(new LocaleList(Locale.FRANCE, Locale.ENGLISH));

    List<String> systemLanguageTags = mSystemLanguagesProvider.getSystemLanguageTags();

    assertThat(systemLanguageTags).containsExactly("fr", "en");
  }
}
