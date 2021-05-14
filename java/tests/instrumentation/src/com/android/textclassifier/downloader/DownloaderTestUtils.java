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

import android.content.Context;
import androidx.work.ListenableWorker;
import androidx.work.WorkInfo;
import androidx.work.WorkManager;
import androidx.work.WorkQuery;
import androidx.work.WorkerParameters;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.Iterables;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import java.io.File;
import java.util.List;

/** Utils for downloader logic testing. */
final class DownloaderTestUtils {

  /** One unique queue holds at most one request at one time. Returns null if no WorkInfo found. */
  public static WorkInfo queryTheOnlyWorkInfo(WorkManager workManager, String queueName)
      throws Exception {
    WorkQuery workQuery =
        WorkQuery.Builder.fromUniqueWorkNames(ImmutableList.of(queueName)).build();
    List<WorkInfo> workInfos = workManager.getWorkInfos(workQuery).get();
    if (workInfos.isEmpty()) {
      return null;
    } else {
      return Iterables.getOnlyElement(workInfos);
    }
  }

  /**
   * Completes immediately with the pre-set result. If it's not retry, the result will also include
   * the input Data as its output Data.
   */
  public static final class TestWorker extends ListenableWorker {
    private static Result expectedResult;

    public TestWorker(Context context, WorkerParameters workerParams) {
      super(context, workerParams);
    }

    @Override
    public ListenableFuture<ListenableWorker.Result> startWork() {
      if (expectedResult == null) {
        return Futures.immediateFailedFuture(new Exception("no expected result"));
      }
      ListenableWorker.Result result;
      switch (expectedResult) {
        case SUCCESS:
          result = ListenableWorker.Result.success(getInputData());
          break;
        case FAILURE:
          result = ListenableWorker.Result.failure(getInputData());
          break;
        case RETRY:
          result = ListenableWorker.Result.retry();
          break;
        default:
          throw new IllegalStateException("illegal result");
      }
      // Reset expected result
      expectedResult = null;
      return Futures.immediateFuture(result);
    }

    /** Sets the expected worker result in a static variable. Will be cleaned up after reading. */
    public static void setExpectedResult(Result expectedResult) {
      TestWorker.expectedResult = expectedResult;
    }

    public enum Result {
      SUCCESS,
      FAILURE,
      RETRY;
    }
  }

  // MoreFiles#deleteRecursively is not available for Android guava.
  public static void deleteRecursively(File f) {
    if (f.isDirectory()) {
      for (File innerFile : f.listFiles()) {
        deleteRecursively(innerFile);
      }
    }
    f.delete();
  }

  private DownloaderTestUtils() {}
}
