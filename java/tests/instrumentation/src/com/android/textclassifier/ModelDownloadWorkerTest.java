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
import static org.testng.Assert.expectThrows;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import androidx.work.ListenableWorker;
import androidx.work.WorkerFactory;
import androidx.work.WorkerParameters;
import androidx.work.testing.TestListenableWorkerBuilder;
import androidx.work.testing.WorkManagerTestInitHelper;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class ModelDownloadWorkerTest {
  private static final String MODEL_URL =
      "http://www.gstatic.com/android/text_classifier/q/v711/en.fb";
  private static final String MODEL_CONTENT = "content";
  private static final String MODEL_CONTENT_CORRUPTED = "CONTENT";
  private static final long MODEL_SIZE_IN_BYTES = 7L;
  private static final String MODEL_FINGERPRINT =
      "5406ebea1618e9b73a7290c5d716f0b47b4f1fbc5d8c"
          + "5e78c9010a3e01c18d8594aa942e3536f7e01574245d34647523";
  private static final int WORKER_MAX_DOWNLOAD_ATTEMPTS = 5;

  private File manifestFile;
  private File pendingModelFile;
  private File targetModelFile;

  @Before
  public void setUp() {
    Context context = ApplicationProvider.getApplicationContext();
    WorkManagerTestInitHelper.initializeTestWorkManager(context);

    this.manifestFile = new File(context.getCacheDir(), "model.fb.manifest");
    this.pendingModelFile = new File(context.getCacheDir(), "model.fb.pending");
    this.targetModelFile = new File(context.getCacheDir(), "model.fb");
  }

  @After
  public void tearDown() {
    manifestFile.delete();
    pendingModelFile.delete();
    targetModelFile.delete();
  }

  @Test
  public void passedVerificationAndMoved() throws Exception {
    ModelDownloadWorker worker = createWorker(manifestFile, pendingModelFile, targetModelFile);
    manifestFile.createNewFile();
    writeToFile(pendingModelFile, MODEL_CONTENT);

    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(targetModelFile.exists()).isTrue();
    assertThat(pendingModelFile.exists()).isFalse();
    assertThat(manifestFile.exists()).isFalse();
  }

  @Test
  public void passedVerificationAndReplaced() throws Exception {
    ModelDownloadWorker worker = createWorker(manifestFile, pendingModelFile, targetModelFile);
    manifestFile.createNewFile();
    writeToFile(pendingModelFile, MODEL_CONTENT);
    writeToFile(targetModelFile, MODEL_CONTENT);

    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(targetModelFile.exists()).isTrue();
    assertThat(pendingModelFile.exists()).isFalse();
    assertThat(manifestFile.exists()).isFalse();
  }

  @Test
  public void failedVerificationAndRetry() throws Exception {
    ModelDownloadWorker worker = createWorker(manifestFile, pendingModelFile, targetModelFile);
    manifestFile.createNewFile();
    writeToFile(pendingModelFile, /* content= */ "");

    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    assertThat(targetModelFile.exists()).isFalse();
    assertThat(pendingModelFile.exists()).isFalse();
    assertThat(manifestFile.exists()).isTrue();
  }

  @Test
  public void validateModel_validationPassed() throws Exception {
    ModelDownloadWorker worker = createWorker(manifestFile, pendingModelFile, targetModelFile);
    writeToFile(pendingModelFile, MODEL_CONTENT);
    worker.handleDownloadedFile(pendingModelFile);
  }

  @Test
  public void validateModel_fileDoesNotExist() throws Exception {
    ModelDownloadWorker worker = createWorker(manifestFile, pendingModelFile, targetModelFile);
    pendingModelFile.delete();
    IllegalStateException e =
        expectThrows(
            IllegalStateException.class, () -> worker.handleDownloadedFile(pendingModelFile));
    assertThat(e).hasCauseThat().hasMessageThat().contains("does not exist");
  }

  @Test
  public void validateModel_emptyFile() throws Exception {
    ModelDownloadWorker worker = createWorker(manifestFile, pendingModelFile, targetModelFile);
    writeToFile(pendingModelFile, /* content= */ "");
    IllegalStateException e =
        expectThrows(
            IllegalStateException.class, () -> worker.handleDownloadedFile(pendingModelFile));
    assertThat(e).hasCauseThat().hasMessageThat().contains("size does not match");
  }

  @Test
  public void validateModel_corruptedContent() throws Exception {
    ModelDownloadWorker worker = createWorker(manifestFile, pendingModelFile, targetModelFile);
    writeToFile(pendingModelFile, MODEL_CONTENT_CORRUPTED);
    IllegalStateException e =
        expectThrows(
            IllegalStateException.class, () -> worker.handleDownloadedFile(pendingModelFile));
    assertThat(e).hasCauseThat().hasMessageThat().contains("fingerprint does not match");
  }

  private static ModelDownloadWorker createWorker(
      File manifestFile, File pendingModelFile, File targetModelFile) {
    return TestListenableWorkerBuilder.from(
            ApplicationProvider.getApplicationContext(), ModelDownloadWorker.class)
        .setInputData(
            ModelDownloadWorker.createInputData(
                MODEL_URL,
                MODEL_SIZE_IN_BYTES,
                MODEL_FINGERPRINT,
                manifestFile.getAbsolutePath(),
                pendingModelFile.getAbsolutePath(),
                targetModelFile.getAbsolutePath(),
                WORKER_MAX_DOWNLOAD_ATTEMPTS,
                /* reuseExistingModelFile= */ true))
        .setRunAttemptCount(0)
        .setWorkerFactory(
            new WorkerFactory() {
              @Override
              public ListenableWorker createWorker(
                  Context appContext, String workerClassName, WorkerParameters workerParameters) {
                return new ModelDownloadWorker(appContext, workerParameters);
              }
            })
        .build();
  }

  private static void writeToFile(File file, String content) throws IOException {
    file.createNewFile();
    FileWriter fileWriter = new FileWriter(file);
    fileWriter.write(content, /* off= */ 0, content.length());
    fileWriter.close();
  }
}
