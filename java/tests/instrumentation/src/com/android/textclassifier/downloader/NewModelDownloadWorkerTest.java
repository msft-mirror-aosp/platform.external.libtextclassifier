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
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import androidx.work.ListenableWorker;
import androidx.work.WorkerFactory;
import androidx.work.WorkerParameters;
import androidx.work.testing.TestListenableWorkerBuilder;
import com.android.os.AtomsProto.TextClassifierDownloadReported;
import com.android.os.AtomsProto.TextClassifierDownloadReported.DownloadStatus;
import com.android.os.AtomsProto.TextClassifierDownloadReported.FailureReason;
import com.android.textclassifier.common.ModelType;
import com.android.textclassifier.common.statsd.TextClassifierDownloadLoggerTestRule;
import com.google.common.collect.Iterables;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.MoreExecutors;
import java.io.File;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(JUnit4.class)
public final class NewModelDownloadWorkerTest {
  private static final String MODEL_TYPE = ModelType.ANNOTATOR;
  private static final TextClassifierDownloadReported.ModelType MODEL_TYPE_ATOM =
      TextClassifierDownloadReported.ModelType.ANNOTATOR;
  private static final String LOCALE_TAG = "en";
  private static final String MANIFEST_URL =
      "https://www.gstatic.com/android/text_classifier/q/v711/en.fb.manifest";
  private static final String MODEL_URL =
      "https://www.gstatic.com/android/text_classifier/q/v711/en.fb";
  private static final int RUN_ATTEMPT_COUNT = 1;
  private static final int MAX_RUN_ATTEMPT_COUNT = 5;
  private static final ModelManifest.Model MODEL_PROTO =
      ModelManifest.Model.newBuilder()
          .setUrl(MODEL_URL)
          .setSizeInBytes(1)
          .setFingerprint("fingerprint")
          .build();
  private static final ModelManifest MODEL_MANIFEST_PROTO =
      ModelManifest.newBuilder().addModels(MODEL_PROTO).build();
  private static final ModelDownloadException FAILED_TO_DOWNLOAD_EXCEPTION =
      new ModelDownloadException(
          ModelDownloadException.FAILED_TO_DOWNLOAD_OTHER, "failed to download");
  private static final FailureReason FAILED_TO_DOWNLOAD_FAILURE_REASON =
      TextClassifierDownloadReported.FailureReason.FAILED_TO_DOWNLOAD_OTHER;

  private File downloadedModelDir;
  private File pendingModelDir;
  private File targetModelFile;

  @Mock private ModelDownloader modelDownloader;
  @Mock private Runnable postDownloadCleanUpRunnable;

  @Rule
  public final TextClassifierDownloadLoggerTestRule loggerTestRule =
      new TextClassifierDownloadLoggerTestRule();

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);

    Context context = ApplicationProvider.getApplicationContext();
    this.downloadedModelDir = new File(context.getCacheDir(), "downloaded");
    this.downloadedModelDir.mkdirs();
    this.pendingModelDir = new File(context.getCacheDir(), "pending");
    this.pendingModelDir.mkdirs();
    this.targetModelFile =
        new File(
            downloadedModelDir,
            NewModelDownloadWorker.formatFileNameByModelTypeAndUrl(MODEL_TYPE, MODEL_URL));
    this.targetModelFile.delete();
  }

  @After
  public void cleanUp() {
    DownloaderTestUtils.deleteRecursively(downloadedModelDir);
    DownloaderTestUtils.deleteRecursively(pendingModelDir);
  }

  @Test
  public void downloadManifest_succeed_downloadModel_succeed_moveModelFile_succeed()
      throws Exception {
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    File pendingModelFile = new File(pendingModelDir, "pending.model.file");
    pendingModelFile.createNewFile();
    when(modelDownloader.downloadModel(MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(pendingModelFile));

    NewModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(targetModelFile.exists()).isTrue();
    assertThat(pendingModelFile.exists()).isFalse();
    verify(postDownloadCleanUpRunnable).run();
    verifyLoggedAtom(DownloadStatus.SUCCEEDED, RUN_ATTEMPT_COUNT, /* failureReason= */ null);
  }

  @Test
  public void downloadManifest_failed() throws Exception {
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFailedFuture(FAILED_TO_DOWNLOAD_EXCEPTION));

    NewModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    assertThat(targetModelFile.exists()).isFalse();
    verify(postDownloadCleanUpRunnable).run();
    verifyLoggedAtom(
        DownloadStatus.FAILED_AND_RETRY, RUN_ATTEMPT_COUNT, FAILED_TO_DOWNLOAD_FAILURE_REASON);
  }

  @Test
  public void downloadManifest_succeed_downloadModel_failed() throws Exception {
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadModel(MODEL_PROTO))
        .thenReturn(Futures.immediateFailedFuture(FAILED_TO_DOWNLOAD_EXCEPTION));

    NewModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
    assertThat(targetModelFile.exists()).isFalse();
    verify(postDownloadCleanUpRunnable).run();
    verifyLoggedAtom(
        DownloadStatus.FAILED_AND_RETRY, RUN_ATTEMPT_COUNT, FAILED_TO_DOWNLOAD_FAILURE_REASON);
  }

  @Test
  public void downloadManifest_succeed_downloadModel_alreadyExist() throws Exception {
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    when(modelDownloader.downloadModel(MODEL_PROTO))
        .thenReturn(Futures.immediateFailedFuture(FAILED_TO_DOWNLOAD_EXCEPTION));
    targetModelFile.createNewFile();

    NewModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.success());
    assertThat(targetModelFile.exists()).isTrue();
    verify(postDownloadCleanUpRunnable).run();
    verifyLoggedAtom(DownloadStatus.SUCCEEDED, RUN_ATTEMPT_COUNT, /* failureReason= */ null);
  }

  @Test
  public void downloadManifest_succeed_downloadModel_succeed_moveModelFile_failed()
      throws Exception {
    when(modelDownloader.downloadManifest(MANIFEST_URL))
        .thenReturn(Futures.immediateFuture(MODEL_MANIFEST_PROTO));
    File pendingModelFile = new File(pendingModelDir, "pending.model.file");
    pendingModelFile.createNewFile();
    when(modelDownloader.downloadModel(MODEL_PROTO))
        .thenReturn(Futures.immediateFuture(pendingModelFile));

    try {
      downloadedModelDir.setWritable(false);

      NewModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
      assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.retry());
      assertThat(targetModelFile.exists()).isFalse();
      assertThat(pendingModelFile.exists()).isFalse();
      verify(postDownloadCleanUpRunnable).run();
      verifyLoggedAtom(
          DownloadStatus.FAILED_AND_RETRY, RUN_ATTEMPT_COUNT, FailureReason.FAILED_TO_MOVE_MODEL);
    } finally {
      downloadedModelDir.setWritable(true);
    }
  }

  @Test
  public void reachMaxRunAttempts() throws Exception {
    NewModelDownloadWorker worker = createWorker(MAX_RUN_ATTEMPT_COUNT);
    assertThat(worker.startWork().get()).isEqualTo(ListenableWorker.Result.failure());
    verifyLoggedAtom(
        DownloadStatus.FAILED_AND_ABORT,
        MAX_RUN_ATTEMPT_COUNT,
        FailureReason.UNKNOWN_FAILURE_REASON);
  }

  @Test
  public void workerStopped() throws Exception {
    NewModelDownloadWorker worker = createWorker(RUN_ATTEMPT_COUNT);
    worker.onStopped();
    verifyLoggedAtom(
        DownloadStatus.FAILED_AND_RETRY, RUN_ATTEMPT_COUNT, FailureReason.WORKER_STOPPED);
  }

  private NewModelDownloadWorker createWorker(int runAttemptCount) {
    return TestListenableWorkerBuilder.from(
            ApplicationProvider.getApplicationContext(), NewModelDownloadWorker.class)
        .setInputData(
            NewModelDownloadWorker.createInputData(
                MODEL_TYPE, LOCALE_TAG, MANIFEST_URL, MAX_RUN_ATTEMPT_COUNT))
        .setRunAttemptCount(runAttemptCount)
        .setWorkerFactory(
            new WorkerFactory() {
              @Override
              public ListenableWorker createWorker(
                  Context appContext, String workerClassName, WorkerParameters workerParameters) {
                return new NewModelDownloadWorker(
                    appContext,
                    workerParameters,
                    MoreExecutors.newDirectExecutorService(),
                    modelDownloader,
                    downloadedModelDir,
                    postDownloadCleanUpRunnable);
              }
            })
        .build();
  }

  private void verifyLoggedAtom(
      DownloadStatus downloadStatus, long runAttemptCount, FailureReason failureReason)
      throws Exception {
    TextClassifierDownloadReported atom = Iterables.getOnlyElement(loggerTestRule.getLoggedAtoms());
    assertThat(atom.getDownloadStatus()).isEqualTo(downloadStatus);
    assertThat(atom.getModelType()).isEqualTo(MODEL_TYPE_ATOM);
    assertThat(atom.getUrlSuffix()).isEqualTo(MANIFEST_URL);
    assertThat(atom.getRunAttemptCount()).isEqualTo(runAttemptCount);
    if (failureReason != null) {
      assertThat(atom.getFailureReason()).isEqualTo(failureReason);
    }
  }
}
