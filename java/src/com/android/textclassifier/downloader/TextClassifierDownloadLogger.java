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

import android.text.TextUtils;
import com.android.textclassifier.common.ModelType;
import com.android.textclassifier.common.ModelType.ModelTypeDef;
import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.common.statsd.TextClassifierStatsLog;
import com.android.textclassifier.downloader.ModelDownloadException.ErrorCode;
import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableMap;

/** Logs TextClassifier download event. */
final class TextClassifierDownloadLogger {
  private static final String TAG = "TextClassifierDownloadLogger";

  // Values for TextClassifierDownloadReported.download_status
  private static final int DOWNLOAD_STATUS_SCHEDULED =
      TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED__DOWNLOAD_STATUS__SCHEDULED;
  private static final int DOWNLOAD_STATUS_SUCCEEDED =
      TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED__DOWNLOAD_STATUS__SUCCEEDED;
  private static final int DOWNLOAD_STATUS_FAILED_AND_RETRY =
      TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED__DOWNLOAD_STATUS__FAILED_AND_RETRY;
  private static final int DOWNLOAD_STATUS_FAILED_AND_ABORT =
      TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED__DOWNLOAD_STATUS__FAILED_AND_ABORT;

  private static final int DEFAULT_MODEL_TYPE =
      TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED__MODEL_TYPE__UNKNOWN_MODEL_TYPE;
  private static final ImmutableMap<String, Integer> MODEL_TYPE_MAP =
      ImmutableMap.of(
          ModelType.ANNOTATOR,
              TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED__MODEL_TYPE__ANNOTATOR,
          ModelType.LANG_ID,
              TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED__MODEL_TYPE__LANG_ID,
          ModelType.ACTIONS_SUGGESTIONS,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__MODEL_TYPE__ACTIONS_SUGGESTIONS);

  private static final int DEFAULT_FILE_TYPE =
      TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FILE_TYPE__UNKNOWN_FILE_TYPE;

  private static final int DEFAULT_FAILURE_REASON =
      TextClassifierStatsLog
          .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__UNKNOWN_FAILURE_REASON;
  private static final ImmutableMap<Integer, Integer> FAILURE_REASON_MAP =
      ImmutableMap.<Integer, Integer>builder()
          .put(
              ModelDownloadException.UNKNOWN_FAILURE_REASON,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__UNKNOWN_FAILURE_REASON)
          .put(
              ModelDownloadException.FAILED_TO_SCHEDULE,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__FAILED_TO_SCHEDULE)
          .put(
              ModelDownloadException.FAILED_TO_DOWNLOAD_SERVICE_CONN_BROKEN,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__FAILED_TO_DOWNLOAD_SERVICE_CONN_BROKEN)
          .put(
              ModelDownloadException.FAILED_TO_DOWNLOAD_404_ERROR,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__FAILED_TO_DOWNLOAD_404_ERROR)
          .put(
              ModelDownloadException.FAILED_TO_DOWNLOAD_OTHER,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__FAILED_TO_DOWNLOAD_OTHER)
          .put(
              ModelDownloadException.DOWNLOADED_FILE_MISSING,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__DOWNLOADED_FILE_MISSING)
          .put(
              ModelDownloadException.FAILED_TO_PARSE_MANIFEST,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__FAILED_TO_PARSE_MANIFEST)
          .put(
              ModelDownloadException.FAILED_TO_VALIDATE_MODEL,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__FAILED_TO_VALIDATE_MODEL)
          .put(
              ModelDownloadException.FAILED_TO_MOVE_MODEL,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__FAILED_TO_MOVE_MODEL)
          .put(
              ModelDownloadException.WORKER_STOPPED,
              TextClassifierStatsLog
                  .TEXT_CLASSIFIER_DOWNLOAD_REPORTED__FAILURE_REASON__WORKER_STOPPED)
          .build();

  /** Logs a scheduled download task. */
  public static void downloadSceduled(@ModelTypeDef String modelType, String url) {
    Preconditions.checkArgument(!TextUtils.isEmpty(url), "url cannot be null/empty");
    TextClassifierStatsLog.write(
        TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED,
        MODEL_TYPE_MAP.getOrDefault(modelType, DEFAULT_MODEL_TYPE),
        DEFAULT_FILE_TYPE,
        DOWNLOAD_STATUS_SCHEDULED,
        url,
        DEFAULT_FAILURE_REASON,
        /* runAttemptCount */ 0);
    if (TcLog.ENABLE_FULL_LOGGING) {
      TcLog.v(
          TAG,
          String.format(
              "Download Reported: modelType=%s, fileType=%d, status=%d, url=%s, "
                  + "failureReason=%d, runAttemptCount=%d",
              MODEL_TYPE_MAP.getOrDefault(modelType, DEFAULT_MODEL_TYPE),
              DEFAULT_FILE_TYPE,
              DOWNLOAD_STATUS_SCHEDULED,
              url,
              DEFAULT_FAILURE_REASON,
              /* runAttemptCount */ 0));
    }
  }

  /** Logs a succeeded download task. */
  public static void downloadSucceeded(
      @ModelTypeDef String modelType, String url, int runAttemptCount) {
    Preconditions.checkArgument(!TextUtils.isEmpty(url), "url cannot be null/empty");
    TextClassifierStatsLog.write(
        TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED,
        MODEL_TYPE_MAP.getOrDefault(modelType, DEFAULT_MODEL_TYPE),
        DEFAULT_FILE_TYPE,
        DOWNLOAD_STATUS_SUCCEEDED,
        url,
        DEFAULT_FAILURE_REASON,
        runAttemptCount);
    if (TcLog.ENABLE_FULL_LOGGING) {
      TcLog.v(
          TAG,
          String.format(
              "Download Reported: modelType=%s, fileType=%d, status=%d, url=%s, "
                  + "failureReason=%d, runAttemptCount=%d",
              MODEL_TYPE_MAP.getOrDefault(modelType, DEFAULT_MODEL_TYPE),
              DEFAULT_FILE_TYPE,
              DOWNLOAD_STATUS_SUCCEEDED,
              url,
              DEFAULT_FAILURE_REASON,
              runAttemptCount));
    }
  }

  /** Logs a failed download task which will be retried later. */
  public static void downloadFailedAndRetry(
      @ModelTypeDef String modelType, String url, @ErrorCode int errorCode, int runAttemptCount) {
    Preconditions.checkArgument(!TextUtils.isEmpty(url), "url cannot be null/empty");
    TextClassifierStatsLog.write(
        TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED,
        MODEL_TYPE_MAP.getOrDefault(modelType, DEFAULT_MODEL_TYPE),
        DEFAULT_FILE_TYPE,
        DOWNLOAD_STATUS_FAILED_AND_RETRY,
        url,
        FAILURE_REASON_MAP.getOrDefault(errorCode, DEFAULT_FAILURE_REASON),
        runAttemptCount);
    if (TcLog.ENABLE_FULL_LOGGING) {
      TcLog.v(
          TAG,
          String.format(
              "Download Reported: modelType=%s, fileType=%d, status=%d, url=%s, "
                  + "failureReason=%d, runAttemptCount=%d",
              MODEL_TYPE_MAP.getOrDefault(modelType, DEFAULT_MODEL_TYPE),
              DEFAULT_FILE_TYPE,
              DOWNLOAD_STATUS_FAILED_AND_RETRY,
              url,
              FAILURE_REASON_MAP.getOrDefault(errorCode, DEFAULT_FAILURE_REASON),
              runAttemptCount));
    }
  }

  /** Logs a failed download task which will not be retried. */
  public static void downloadFailedAndAbort(
      @ModelTypeDef String modelType, String url, @ErrorCode int errorCode, int runAttemptCount) {
    Preconditions.checkArgument(!TextUtils.isEmpty(url), "url cannot be null/empty");
    TextClassifierStatsLog.write(
        TextClassifierStatsLog.TEXT_CLASSIFIER_DOWNLOAD_REPORTED,
        MODEL_TYPE_MAP.getOrDefault(modelType, DEFAULT_MODEL_TYPE),
        DEFAULT_FILE_TYPE,
        DOWNLOAD_STATUS_FAILED_AND_ABORT,
        url,
        FAILURE_REASON_MAP.getOrDefault(errorCode, DEFAULT_FAILURE_REASON),
        runAttemptCount);
    if (TcLog.ENABLE_FULL_LOGGING) {
      TcLog.v(
          TAG,
          String.format(
              "Download Reported: modelType=%s, fileType=%d, status=%d, url=%s, "
                  + "failureReason=%d, runAttemptCount=%d",
              MODEL_TYPE_MAP.getOrDefault(modelType, DEFAULT_MODEL_TYPE),
              DEFAULT_FILE_TYPE,
              DOWNLOAD_STATUS_FAILED_AND_ABORT,
              url,
              FAILURE_REASON_MAP.getOrDefault(errorCode, DEFAULT_FAILURE_REASON),
              runAttemptCount));
    }
  }

  private TextClassifierDownloadLogger() {}
}
