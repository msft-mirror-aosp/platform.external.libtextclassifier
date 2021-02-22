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

import com.google.common.util.concurrent.ListenableFuture;
import java.io.File;
import java.net.URI;

/** Interface for downloading files from certain URI. */
public interface ModelDownloader {

  /**
   * Downloads a file from the given URI to the target file.
   *
   * <p>For a successful download, this method returns a Future containing the number of bytes
   * written. For a failure case, the Future would fail, with the exception containing more
   * information. The implementations of this interface should clean up unfinished model files if
   * the download fails.
   *
   * @param uri the URI to download file from
   * @param targetFile the target File to write the downloaded content. If the file already exists,
   *     its content will be cleared
   */
  ListenableFuture<Long> download(URI uri, File targetFile);
}
