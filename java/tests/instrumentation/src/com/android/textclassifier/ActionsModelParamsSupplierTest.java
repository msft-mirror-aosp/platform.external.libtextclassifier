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

package com.android.textclassifier;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import java.io.File;
import java.util.Collections;
import java.util.Locale;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ActionsModelParamsSupplierTest {

  @Test
  public void getSerializedPreconditions_validActionsModelParams() {
    ModelFileManager.ModelFile modelFile =
        new ModelFileManager.ModelFile(
            new File("/model/file"),
            200 /* version */,
            Collections.singletonList(Locale.forLanguageTag("en")),
            "en",
            false);
    byte[] serializedPreconditions = new byte[] {0x12, 0x24, 0x36};
    ActionsModelParamsSupplier.ActionsModelParams params =
        new ActionsModelParamsSupplier.ActionsModelParams(
            200 /* version */, "en", serializedPreconditions);

    byte[] actual = params.getSerializedPreconditions(modelFile);

    assertThat(actual).isEqualTo(serializedPreconditions);
  }

  @Test
  public void getSerializedPreconditions_invalidVersion() {
    ModelFileManager.ModelFile modelFile =
        new ModelFileManager.ModelFile(
            new File("/model/file"),
            201 /* version */,
            Collections.singletonList(Locale.forLanguageTag("en")),
            "en",
            false);
    byte[] serializedPreconditions = new byte[] {0x12, 0x24, 0x36};
    ActionsModelParamsSupplier.ActionsModelParams params =
        new ActionsModelParamsSupplier.ActionsModelParams(
            200 /* version */, "en", serializedPreconditions);

    byte[] actual = params.getSerializedPreconditions(modelFile);

    assertThat(actual).isNull();
  }

  @Test
  public void getSerializedPreconditions_invalidLocales() {
    final String languageTag = "zh";
    ModelFileManager.ModelFile modelFile =
        new ModelFileManager.ModelFile(
            new File("/model/file"),
            200 /* version */,
            Collections.singletonList(Locale.forLanguageTag(languageTag)),
            languageTag,
            false);
    byte[] serializedPreconditions = new byte[] {0x12, 0x24, 0x36};
    ActionsModelParamsSupplier.ActionsModelParams params =
        new ActionsModelParamsSupplier.ActionsModelParams(
            200 /* version */, "en", serializedPreconditions);

    byte[] actual = params.getSerializedPreconditions(modelFile);

    assertThat(actual).isNull();
  }
}
