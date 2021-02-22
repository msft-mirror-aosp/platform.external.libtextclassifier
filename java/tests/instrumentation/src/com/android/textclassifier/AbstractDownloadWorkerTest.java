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

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import androidx.work.Data;
import androidx.work.ListenableWorker;
import androidx.work.WorkerFactory;
import androidx.work.WorkerParameters;
import androidx.work.testing.TestListenableWorkerBuilder;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.MoreExecutors;
import java.io.File;
import java.io.FileWriter;
import java.net.URI;
import java.util.concurrent.ExecutorService;
import java.util.function.Function;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class AbstractDownloadWorkerTest {
  private static final String URL = "http://www.gstatic.com/android/text_classifier/q/v711/en.fb";
  private static final String CONTENT_BYTES = "abc";
  private static final int WORKER_MAX_DOWNLOAD_ATTEMPTS = 5;
  private static final Function<File, Void> NO_OP_HANDLE_FUNC = f -> null;

  private File targetModelFile;

  @Before
  public void setUp() {
    this.targetModelFile =
        new File(ApplicationProvider.getApplicationContext().getCacheDir(), "model.fb");
    targetModelFile.deleteOnExit();
  }

  @Test
  public void download_succeeded() throws Exception {
    AbstractDownloadWorker worker =
        createWorker(
            createData(/* reuseExistingFile= */ false),
            /* runAttemptCount= */ 0,
            TestModelDownloader.withSuccess(CONTENT_BYTES),
            NO_OP_HANDLE_FUNC);
    targetModelFile.delete();

    assertThat(targetModelFile.exists()).isFalse();
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(targetModelFile.exists()).isTrue();
  }

  @Test
  public void download_reuseExistingFile() throws Exception {
    AbstractDownloadWorker worker =
        createWorker(
            createData(/* reuseExistingFile= */ true),
            /* runAttemptCount= */ 0,
            // If we reuse existing file, downloader will not be invoked, thus won't fail
            TestModelDownloader.withFailure(new Exception()),
            NO_OP_HANDLE_FUNC);
    targetModelFile.createNewFile();

    assertThat(targetModelFile.exists()).isTrue();
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(targetModelFile.exists()).isTrue();
  }

  @Test
  public void download_reuseExistingFileButNotExist() throws Exception {
    AbstractDownloadWorker worker =
        createWorker(
            createData(/* reuseExistingFile= */ true),
            /* runAttemptCount= */ 0,
            TestModelDownloader.withSuccess(CONTENT_BYTES),
            NO_OP_HANDLE_FUNC);
    targetModelFile.delete();

    assertThat(targetModelFile.exists()).isFalse();
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(targetModelFile.exists()).isTrue();
  }

  @Test
  public void download_reuseExistingFileButNotExistAndFails() throws Exception {
    AbstractDownloadWorker worker =
        createWorker(
            createData(/* reuseExistingFile= */ true),
            /* runAttemptCount= */ 0,
            TestModelDownloader.withFailure(new Exception()),
            NO_OP_HANDLE_FUNC);
    targetModelFile.delete();

    assertThat(targetModelFile.exists()).isFalse();
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    assertThat(targetModelFile.exists()).isFalse();
  }

  @Test
  public void download_failedAndRetry() throws Exception {
    AbstractDownloadWorker worker =
        createWorker(
            createData(/* reuseExistingFile= */ false),
            /* runAttemptCount= */ 0,
            TestModelDownloader.withFailure(new Exception()),
            NO_OP_HANDLE_FUNC);
    targetModelFile.delete();

    assertThat(targetModelFile.exists()).isFalse();
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    assertThat(targetModelFile.exists()).isFalse();
  }

  @Test
  public void download_failedTooManyAttempts() throws Exception {
    AbstractDownloadWorker worker =
        createWorker(
            createData(/* reuseExistingFile= */ false),
            WORKER_MAX_DOWNLOAD_ATTEMPTS,
            TestModelDownloader.withSuccess(CONTENT_BYTES),
            NO_OP_HANDLE_FUNC);
    targetModelFile.delete();

    assertThat(targetModelFile.exists()).isFalse();
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.failure());
    assertThat(targetModelFile.exists()).isFalse();
  }

  @Test
  public void download_errorWhenHandlingDownloadedFile() throws Exception {
    AbstractDownloadWorker worker =
        createWorker(
            createData(/* reuseExistingFile= */ false),
            /* runAttemptCount= */ 0,
            TestModelDownloader.withSuccess(""),
            file -> {
              throw new RuntimeException();
            });
    targetModelFile.delete();

    assertThat(targetModelFile.exists()).isFalse();
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    // Downlaoded file should be cleaned up if hanlding function fails
    assertThat(targetModelFile.exists()).isFalse();
  }

  private Data createData(boolean reuseExistingFile) {
    return AbstractDownloadWorker.createInputDataBuilder(
            URL, targetModelFile.getAbsolutePath(), reuseExistingFile, WORKER_MAX_DOWNLOAD_ATTEMPTS)
        .build();
  }

  private static AbstractDownloadWorker createWorker(
      Data data, int runAttemptCount, ModelDownloader downloader, Function<File, Void> handleFunc) {
    return TestListenableWorkerBuilder.from(
            ApplicationProvider.getApplicationContext(), TestDownloadWorker.class)
        .setInputData(data)
        .setRunAttemptCount(runAttemptCount)
        .setWorkerFactory(
            new WorkerFactory() {
              @Override
              public ListenableWorker createWorker(
                  Context appContext, String workerClassName, WorkerParameters workerParameters) {
                return new TestDownloadWorker(
                    appContext,
                    workerParameters,
                    MoreExecutors.newDirectExecutorService(),
                    downloader,
                    handleFunc);
              }
            })
        .build();
  }

  /** A test AbstractDownloadWorker impl which handles downloaded file with a given Function. */
  private static class TestDownloadWorker extends AbstractDownloadWorker {
    private final Function<File, Void> handleFunc;

    TestDownloadWorker(
        Context context,
        WorkerParameters workerParameters,
        ExecutorService bgExecutorService,
        ModelDownloader modelDownloader,
        Function<File, Void> handleFunc) {
      super(context, workerParameters, bgExecutorService, modelDownloader);

      this.handleFunc = handleFunc;
    }

    @Override
    Void handleDownloadedFile(File downloadedFile) {
      return handleFunc.apply(downloadedFile);
    }
  }

  /** A ModelDownloader implementation for testing. Set expected resilts in its constructor. */
  private static class TestModelDownloader implements ModelDownloader {
    private final String strWrittenToFile;
    private final ListenableFuture<Long> futureToReturn;

    public static TestModelDownloader withSuccess(String strWrittenToFile) {
      return new TestModelDownloader(
          Futures.immediateFuture((long) strWrittenToFile.getBytes().length), strWrittenToFile);
    }

    public static TestModelDownloader withFailure(Throwable throwable) {
      return new TestModelDownloader(Futures.immediateFailedFuture(throwable), null);
    }

    private TestModelDownloader(ListenableFuture<Long> futureToReturn, String strWrittenToFile) {
      this.strWrittenToFile = strWrittenToFile;
      this.futureToReturn = futureToReturn;
    }

    @Override
    public ListenableFuture<Long> download(URI uri, File targetFile) {
      if (strWrittenToFile != null) {
        try {
          targetFile.createNewFile();
          FileWriter fileWriter = new FileWriter(targetFile);
          fileWriter.write(strWrittenToFile, /* off= */ 0, strWrittenToFile.length());
          fileWriter.close();
        } catch (Exception e) {
          throw new RuntimeException("Failed to prepare test downloadeded file.", e);
        }
      }
      return futureToReturn;
    }
  }
}
