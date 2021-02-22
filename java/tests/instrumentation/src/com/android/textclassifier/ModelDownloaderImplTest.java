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
import static java.util.concurrent.TimeUnit.SECONDS;
import static org.testng.Assert.expectThrows;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import com.android.textclassifier.TestModelDownloaderService.DownloadResult;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.MoreExecutors;
import java.io.File;
import java.net.URI;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public final class ModelDownloaderImplTest {
  private static final URI TEST_URI = URI.create("test_uri");

  private ModelDownloaderImpl modelDownloaderImpl;
  private File targetFile;

  @Before
  public void setUp() {
    Context context = ApplicationProvider.getApplicationContext();
    this.modelDownloaderImpl =
        new ModelDownloaderImpl(
            context, MoreExecutors.newDirectExecutorService(), TestModelDownloaderService.class);
    this.targetFile = new File(context.getCacheDir(), "targetFile.fb");
  }

  @After
  public void tearDown() {
    TestModelDownloaderService.reset();
  }

  @Test
  public void download_failToBind() throws Exception {
    assertThat(TestModelDownloaderService.hasEverBeenBound()).isFalse();
    assertThat(TestModelDownloaderService.isBound()).isFalse();

    TestModelDownloaderService.setBindSucceed(false);
    ListenableFuture<Long> bytesWrittenFuture = modelDownloaderImpl.download(TEST_URI, targetFile);

    expectThrows(Throwable.class, bytesWrittenFuture::get);
    assertThat(TestModelDownloaderService.isBound()).isFalse();
    assertThat(TestModelDownloaderService.hasEverBeenBound()).isFalse();
  }

  @Test
  public void download_succeed() throws Exception {
    assertThat(TestModelDownloaderService.hasEverBeenBound()).isFalse();
    assertThat(TestModelDownloaderService.isBound()).isFalse();

    TestModelDownloaderService.setBindSucceed(true);
    TestModelDownloaderService.setDownloadResult(DownloadResult.SUCCEEDED);
    ListenableFuture<Long> bytesWrittenFuture = modelDownloaderImpl.download(TEST_URI, targetFile);

    assertThat(bytesWrittenFuture.get()).isEqualTo(TestModelDownloaderService.BYTES_WRITTEN);
    assertThat(TestModelDownloaderService.getOnUnbindInvokedLatch().await(1L, SECONDS)).isTrue();
    assertThat(TestModelDownloaderService.isBound()).isFalse();
    assertThat(TestModelDownloaderService.hasEverBeenBound()).isTrue();
  }

  @Test
  public void download_fail() throws Exception {
    assertThat(TestModelDownloaderService.hasEverBeenBound()).isFalse();
    assertThat(TestModelDownloaderService.isBound()).isFalse();

    TestModelDownloaderService.setBindSucceed(true);
    TestModelDownloaderService.setDownloadResult(DownloadResult.FAILED);
    ListenableFuture<Long> bytesWrittenFuture = modelDownloaderImpl.download(TEST_URI, targetFile);

    Throwable t = expectThrows(Throwable.class, bytesWrittenFuture::get);
    assertThat(t).hasMessageThat().contains(TestModelDownloaderService.ERROR_MSG);
    assertThat(TestModelDownloaderService.getOnUnbindInvokedLatch().await(1L, SECONDS)).isTrue();
    assertThat(TestModelDownloaderService.isBound()).isFalse();
    assertThat(TestModelDownloaderService.hasEverBeenBound()).isTrue();
  }

  @Test
  public void download_cancelAndUnbind() throws Exception {
    assertThat(TestModelDownloaderService.hasEverBeenBound()).isFalse();
    assertThat(TestModelDownloaderService.isBound()).isFalse();

    TestModelDownloaderService.setBindSucceed(true);
    TestModelDownloaderService.setDownloadResult(DownloadResult.RUNNING_FOREVER);
    ListenableFuture<Long> bytesWrittenFuture = modelDownloaderImpl.download(TEST_URI, targetFile);
    bytesWrittenFuture.cancel(true);

    expectThrows(Throwable.class, bytesWrittenFuture::get);
    assertThat(TestModelDownloaderService.getOnUnbindInvokedLatch().await(1L, SECONDS)).isTrue();
    assertThat(TestModelDownloaderService.isBound()).isFalse();
    assertThat(TestModelDownloaderService.hasEverBeenBound()).isTrue();
  }
}
