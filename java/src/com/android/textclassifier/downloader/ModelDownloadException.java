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

import static java.lang.annotation.RetentionPolicy.SOURCE;

import androidx.annotation.IntDef;
import java.lang.annotation.Retention;

// TODO(licha): Consider making this a checked exception
/** Exception thrown when downloading a model. */
final class ModelDownloadException extends RuntimeException {

  // Consistent with TextClassifierDownloadReported.failure_reason
  public static final int UNKNOWN_FAILURE_REASON = 0;
  public static final int FAILED_TO_SCHEDULE = 1;
  public static final int FAILED_TO_DOWNLOAD_SERVICE_CONN_BROKEN = 2;
  public static final int FAILED_TO_DOWNLOAD_404_ERROR = 3;
  public static final int FAILED_TO_DOWNLOAD_OTHER = 4;
  public static final int DOWNLOADED_FILE_MISSING = 5;
  public static final int FAILED_TO_PARSE_MANIFEST = 6;
  public static final int FAILED_TO_VALIDATE_MODEL = 7;
  public static final int FAILED_TO_MOVE_MODEL = 8;
  public static final int WORKER_STOPPED = 9;

  /** Error code for a failed download task. */
  @Retention(SOURCE)
  @IntDef({
    UNKNOWN_FAILURE_REASON,
    FAILED_TO_SCHEDULE,
    FAILED_TO_DOWNLOAD_SERVICE_CONN_BROKEN,
    FAILED_TO_DOWNLOAD_404_ERROR,
    FAILED_TO_DOWNLOAD_OTHER,
    DOWNLOADED_FILE_MISSING,
    FAILED_TO_PARSE_MANIFEST,
    FAILED_TO_VALIDATE_MODEL,
    FAILED_TO_MOVE_MODEL,
    WORKER_STOPPED
  })
  public @interface ErrorCode {}

  private final int errorCode;

  public ModelDownloadException(@ErrorCode int errorCode, Throwable cause) {
    super(cause);
    this.errorCode = errorCode;
  }

  public ModelDownloadException(@ErrorCode int errorCode, String message) {
    super(message);
    this.errorCode = errorCode;
  }

  @ErrorCode
  public int getErrorCode() {
    return errorCode;
  }
}
