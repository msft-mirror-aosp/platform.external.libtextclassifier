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

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import java.util.concurrent.CountDownLatch;

/** Test Service of IModelDownloaderService. */
public final class TestModelDownloaderService extends Service {
  public static final String GOOD_URI = "good_uri";
  public static final String BAD_URI = "bad_uri";
  public static final long BYTES_WRITTEN = 1L;
  public static final String ERROR_MSG = "not good uri";

  public enum DownloadResult {
    SUCCEEDED,
    FAILED,
    RUNNING_FOREVER,
    DO_NOTHING
  }

  // Obviously this does not work when considering concurrency, but probably fine for test purpose
  private static boolean boundBefore = false;
  private static boolean boundNow = false;
  private static CountDownLatch onUnbindInvokedLatch = new CountDownLatch(1);

  private static boolean bindSucceed = false;
  private static DownloadResult downloadResult = DownloadResult.SUCCEEDED;

  public static boolean hasEverBeenBound() {
    return boundBefore;
  }

  public static boolean isBound() {
    return boundNow;
  }

  public static CountDownLatch getOnUnbindInvokedLatch() {
    return onUnbindInvokedLatch;
  }

  public static void setBindSucceed(boolean bindSucceed) {
    TestModelDownloaderService.bindSucceed = bindSucceed;
  }

  public static void setDownloadResult(DownloadResult result) {
    TestModelDownloaderService.downloadResult = result;
  }

  public static void reset() {
    boundBefore = false;
    boundNow = false;
    onUnbindInvokedLatch = new CountDownLatch(1);
    bindSucceed = false;
  }

  @Override
  public IBinder onBind(Intent intent) {
    if (bindSucceed) {
      boundBefore = true;
      boundNow = true;
      return new TestModelDownloaderServiceImpl();
    } else {
      return null;
    }
  }

  @Override
  public boolean onUnbind(Intent intent) {
    boundNow = false;
    onUnbindInvokedLatch.countDown();
    return false;
  }

  private static final class TestModelDownloaderServiceImpl extends IModelDownloaderService.Stub {
    @Override
    public void download(String uri, String unused, IModelDownloaderCallback callback) {
      try {
        switch (downloadResult) {
          case SUCCEEDED:
            callback.onSuccess(BYTES_WRITTEN);
            break;
          case FAILED:
            callback.onFailure(ERROR_MSG);
            break;
          case RUNNING_FOREVER:
            while (true) {}
          case DO_NOTHING:
            // Do nothing
        }
      } catch (Throwable t) {
        // The test would timeout if failing to get the callback result
      }
    }
  }
}
