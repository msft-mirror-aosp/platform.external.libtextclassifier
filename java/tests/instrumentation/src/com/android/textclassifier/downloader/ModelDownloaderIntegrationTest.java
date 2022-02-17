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

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.util.Log;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassification.Request;
import android.view.textclassifier.TextClassifier;
import androidx.test.platform.app.InstrumentationRegistry;
import com.android.textclassifier.testing.ExtServicesTextClassifierRule;
import com.android.textclassifier.testing.TestingLocaleListOverrideRule;
import java.io.IOException;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class ModelDownloaderIntegrationTest {
  private static final String TAG = "ModelDownloaderTest";
  private static final String EXPERIMENTAL_EN_ANNOTATOR_MANIFEST_URL =
      "https://www.gstatic.com/android/text_classifier/r/experimental/v999999999/en.fb.manifest";
  private static final String EXPERIMENTAL_EN_TAG = "en_v999999999";
  private static final String V804_EN_ANNOTATOR_MANIFEST_URL =
      "https://www.gstatic.com/android/text_classifier/r/v804/en.fb.manifest";
  private static final String V804_RU_ANNOTATOR_MANIFEST_URL =
      "https://www.gstatic.com/android/text_classifier/r/v804/ru.fb.manifest";
  private static final String V804_EN_TAG = "en_v804";
  private static final String V804_RU_TAG = "ru_v804";
  private static final String FACTORY_MODEL_TAG = "*";

  @Rule
  public final TestingLocaleListOverrideRule testingLocaleListOverrideRule =
      new TestingLocaleListOverrideRule();

  @Rule
  public final ExtServicesTextClassifierRule extServicesTextClassifierRule =
      new ExtServicesTextClassifierRule();

  private TextClassifier textClassifier;

  @Before
  public void setup() throws Exception {
    // Flag overrides below can be overridden by Phenotype sync, which makes this test flaky
    runShellCommand("device_config put textclassifier config_updater_model_enabled false");
    runShellCommand("device_config put textclassifier model_download_manager_enabled true");
    runShellCommand("device_config put textclassifier model_download_backoff_delay_in_millis 5");

    textClassifier = extServicesTextClassifierRule.getTextClassifier();
    startExtservicesProcess();
  }

  @After
  public void tearDown() throws Exception {
    runShellCommand("device_config delete textclassifier manifest_url_annotator_en");
    runShellCommand("device_config delete textclassifier manifest_url_annotator_ru");
    runShellCommand("device_config put textclassifier config_updater_model_enabled true");
    runShellCommand("device_config delete textclassifier multi_language_support_enabled");
    runShellCommand(
        "device_config put textclassifier model_download_backoff_delay_in_millis 3600000");
  }

  @Test
  public void smokeTest() throws IOException, InterruptedException {
    runShellCommand(
        "device_config put textclassifier manifest_url_annotator_en "
            + V804_EN_ANNOTATOR_MANIFEST_URL);

    assertWithRetries(
        /* maxAttempts= */ 10, /* sleepMs= */ 1000, () -> verifyActiveEnglishModel(V804_EN_TAG));
  }

  @Test
  public void downgradeModel() throws IOException, InterruptedException {
    // Download an experimental model.
    {
      runShellCommand(
          "device_config put textclassifier manifest_url_annotator_en "
              + EXPERIMENTAL_EN_ANNOTATOR_MANIFEST_URL);

      assertWithRetries(
          /* maxAttempts= */ 10,
          /* sleepMs= */ 1000,
          () -> verifyActiveEnglishModel(EXPERIMENTAL_EN_TAG));
    }

    // Downgrade to an older model.
    {
      runShellCommand(
          "device_config put textclassifier manifest_url_annotator_en "
              + V804_EN_ANNOTATOR_MANIFEST_URL);

      assertWithRetries(
          /* maxAttempts= */ 10, /* sleepMs= */ 1000, () -> verifyActiveEnglishModel(V804_EN_TAG));
    }
  }

  @Test
  public void upgradeModel() throws IOException, InterruptedException {
    // Download a model.
    {
      runShellCommand(
          "device_config put textclassifier manifest_url_annotator_en "
              + V804_EN_ANNOTATOR_MANIFEST_URL);

      assertWithRetries(
          /* maxAttempts= */ 10, /* sleepMs= */ 1000, () -> verifyActiveEnglishModel(V804_EN_TAG));
    }

    // Upgrade to an experimental model.
    {
      runShellCommand(
          "device_config put textclassifier manifest_url_annotator_en "
              + EXPERIMENTAL_EN_ANNOTATOR_MANIFEST_URL);

      assertWithRetries(
          /* maxAttempts= */ 10,
          /* sleepMs= */ 1000,
          () -> verifyActiveEnglishModel(EXPERIMENTAL_EN_TAG));
    }
  }

  @Test
  public void clearFlag() throws IOException, InterruptedException {
    // Download a new model.
    {
      runShellCommand(
          "device_config put textclassifier manifest_url_annotator_en "
              + EXPERIMENTAL_EN_ANNOTATOR_MANIFEST_URL);

      assertWithRetries(
          /* maxAttempts= */ 10,
          /* sleepMs= */ 1000,
          () -> verifyActiveEnglishModel(EXPERIMENTAL_EN_TAG));
    }

    // Revert the flag.
    {
      runShellCommand("device_config delete textclassifier manifest_url_annotator_en");
      // Fallback to use the universal model.
      assertWithRetries(
          /* maxAttempts= */ 10,
          /* sleepMs= */ 1000,
          () -> verifyActiveModel(/* text= */ "abc", /* expectedVersion= */ FACTORY_MODEL_TAG));
    }
  }

  @Test
  public void modelsForMultipleLanguagesDownloaded() throws IOException, InterruptedException {
    runShellCommand("device_config put textclassifier multi_language_support_enabled true");
    testingLocaleListOverrideRule.set("en-US", "ru-RU");

    // download en model
    runShellCommand(
        "device_config put textclassifier manifest_url_annotator_en "
            + EXPERIMENTAL_EN_ANNOTATOR_MANIFEST_URL);

    // download ru model
    runShellCommand(
        "device_config put textclassifier manifest_url_annotator_ru "
            + V804_RU_ANNOTATOR_MANIFEST_URL);
    assertWithRetries(
        /* maxAttempts= */ 10,
        /* sleepMs= */ 1000,
        () -> verifyActiveEnglishModel(EXPERIMENTAL_EN_TAG));

    assertWithRetries(/* maxAttempts= */ 10, /* sleepMs= */ 1000, this::verifyActiveRussianModel);

    assertWithRetries(
        /* maxAttempts= */ 10,
        /* sleepMs= */ 1000,
        () -> verifyActiveModel(/* text= */ "français", /* expectedVersion= */ FACTORY_MODEL_TAG));
  }

  private void assertWithRetries(int maxAttempts, int sleepMs, Runnable assertRunnable)
      throws InterruptedException {
    for (int i = 0; i < maxAttempts; i++) {
      try {
        assertRunnable.run();
        break; // success. Bail out.
      } catch (AssertionError ex) {
        if (i == maxAttempts - 1) { // last attempt, give up.
          throw ex;
        } else {
          Thread.sleep(sleepMs);
        }
      }
    }
  }

  private void verifyActiveModel(String text, String expectedVersion) {
    TextClassification textClassification =
        textClassifier.classifyText(new Request.Builder(text, 0, text.length()).build());
    Log.d(TAG, "verifyActiveModel. TextClassification ID: " + textClassification.getId());
    // The result id contains the name of the just used model.
    assertThat(textClassification.getId()).contains(expectedVersion);
  }

  private void verifyActiveEnglishModel(String expectedVersion) {
    verifyActiveModel("abc", expectedVersion);
  }

  private void verifyActiveRussianModel() {
    verifyActiveModel("привет", V804_RU_TAG);
  }

  private void startExtservicesProcess() {
    // Start the process of ExtServices by sending it a text classifier request.
    textClassifier.classifyText(new TextClassification.Request.Builder("abc", 0, 3).build());
  }

  private static void runShellCommand(String cmd) {
    Log.v(TAG, "run shell command: " + cmd);
    Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
    UiAutomation uiAutomation = instrumentation.getUiAutomation();
    uiAutomation.executeShellCommand(cmd);
  }
}
