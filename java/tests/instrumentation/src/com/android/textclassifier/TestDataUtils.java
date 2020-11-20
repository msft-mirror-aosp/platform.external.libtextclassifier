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

import com.android.textclassifier.ModelFileManager.ModelFile;
import com.android.textclassifier.ModelFileManager.ModelFile.ModelType;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import java.io.File;
import java.util.Locale;
import java.util.function.Supplier;

/** Utils to access test data files. */
public final class TestDataUtils {
  private static final ImmutableMap<String, Supplier<ImmutableList<ModelFile>>>
      MODEL_FILES_SUPPLIER =
          new ImmutableMap.Builder<String, Supplier<ImmutableList<ModelFile>>>()
              .put(
                  ModelType.ANNOTATOR,
                  () ->
                      ImmutableList.of(
                          new ModelFile(
                              ModelType.ANNOTATOR,
                              new File(
                                  TestDataUtils.getTestDataFolder(), "testdata/annotator.model"),
                              711,
                              ImmutableList.of(Locale.ENGLISH),
                              "en",
                              false)))
              .put(
                  ModelType.ACTIONS_SUGGESTIONS,
                  () ->
                      ImmutableList.of(
                          new ModelFile(
                              ModelType.ACTIONS_SUGGESTIONS,
                              new File(TestDataUtils.getTestDataFolder(), "testdata/actions.model"),
                              104,
                              ImmutableList.of(Locale.ENGLISH),
                              "en",
                              false)))
              .put(
                  ModelType.LANG_ID,
                  () ->
                      ImmutableList.of(
                          new ModelFile(
                              ModelType.LANG_ID,
                              new File(TestDataUtils.getTestDataFolder(), "testdata/langid.model"),
                              1,
                              ImmutableList.of(),
                              "*",
                              true)))
              .build();

  /** Returns the root folder that contains the test data. */
  public static File getTestDataFolder() {
    return new File("/data/local/tmp/TextClassifierServiceTest/");
  }

  public static ModelFileManager createModelFileManagerForTesting() {
    return new ModelFileManager(
        /* downloadModelDir= */ TestDataUtils.getTestDataFolder(), MODEL_FILES_SUPPLIER);
  }

  private TestDataUtils() {}
}
