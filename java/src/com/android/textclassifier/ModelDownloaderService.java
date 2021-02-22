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
import com.android.textclassifier.common.base.TcLog;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Service to expose IModelDownloaderService. */
public final class ModelDownloaderService extends Service {
  private static final String TAG = "ModelDownloaderService";

  private ExecutorService executorService;
  private IBinder iBinder;

  @Override
  public void onCreate() {
    super.onCreate();
    // TODO(licha): Use a shared thread pool for IO intensive tasks
    this.executorService = Executors.newSingleThreadExecutor();
    this.iBinder = new ModelDownloaderServiceImpl(executorService);
  }

  @Override
  public IBinder onBind(Intent intent) {
    TcLog.d(TAG, "Binding to ModelDownloadService");
    return iBinder;
  }

  @Override
  public void onDestroy() {
    TcLog.d(TAG, "Destroying ModelDownloadService");
    executorService.shutdown();
  }
}
