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

package com.android.textclassifier.downloader;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.android.os.AtomsProto.TextClassifierDownloadReported;
import com.android.textclassifier.common.ModelType;
import com.android.textclassifier.common.statsd.TextClassifierDownloadLoggerTestRule;
import com.google.common.collect.Iterables;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public final class TextClassifierDownloadLoggerTest {
  private static final String MODEL_TYPE = ModelType.LANG_ID;
  private static final TextClassifierDownloadReported.ModelType MODEL_TYPE_ATOM =
      TextClassifierDownloadReported.ModelType.LANG_ID;
  private static final String URL =
      "https://www.gstatic.com/android/text_classifier/x/v123/en.fb.manifest";
  private static final int ERROR_CODE = ModelDownloadException.FAILED_TO_DOWNLOAD_404_ERROR;
  private static final TextClassifierDownloadReported.FailureReason FAILURE_REASON_ATOM =
      TextClassifierDownloadReported.FailureReason.FAILED_TO_DOWNLOAD_404_ERROR;
  private static final int RUN_ATTEMPT_COUNT = 1;

  @Rule
  public final TextClassifierDownloadLoggerTestRule loggerTestRule =
      new TextClassifierDownloadLoggerTestRule();

  @Test
  public void downloadSucceeded() throws Exception {
    TextClassifierDownloadLogger.downloadSucceeded(MODEL_TYPE, URL, RUN_ATTEMPT_COUNT);

    TextClassifierDownloadReported atom = Iterables.getOnlyElement(loggerTestRule.getLoggedAtoms());
    assertThat(atom.getDownloadStatus())
        .isEqualTo(TextClassifierDownloadReported.DownloadStatus.SUCCEEDED);
    assertThat(atom.getModelType()).isEqualTo(MODEL_TYPE_ATOM);
    assertThat(atom.getUrlSuffix()).isEqualTo(URL);
    assertThat(atom.getRunAttemptCount()).isEqualTo(RUN_ATTEMPT_COUNT);
  }

  @Test
  public void downloadFailedAndRetry() throws Exception {
    TextClassifierDownloadLogger.downloadFailedAndRetry(
        MODEL_TYPE, URL, ERROR_CODE, RUN_ATTEMPT_COUNT);

    TextClassifierDownloadReported atom = Iterables.getOnlyElement(loggerTestRule.getLoggedAtoms());
    assertThat(atom.getDownloadStatus())
        .isEqualTo(TextClassifierDownloadReported.DownloadStatus.FAILED_AND_RETRY);
    assertThat(atom.getModelType()).isEqualTo(MODEL_TYPE_ATOM);
    assertThat(atom.getUrlSuffix()).isEqualTo(URL);
    assertThat(atom.getRunAttemptCount()).isEqualTo(RUN_ATTEMPT_COUNT);
    assertThat(atom.getFailureReason()).isEqualTo(FAILURE_REASON_ATOM);
  }
}
