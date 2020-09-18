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

import android.provider.DeviceConfig;
import com.android.textclassifier.common.base.TcLog;
import java.util.concurrent.Executor;

/** Manager to listen to config update and download latest models. */
final class ModelDownloadManager {
  private static final String TAG = "ModelDownloadManager";

  private final Executor listenerExecutor;
  private final DeviceConfig.OnPropertiesChangedListener deviceConfigListener;

  public ModelDownloadManager(Executor executor) {
    this.listenerExecutor = executor;
    this.deviceConfigListener =
        new DeviceConfig.OnPropertiesChangedListener() {
          @Override
          public void onPropertiesChanged(DeviceConfig.Properties unused) {
            // Trigger the check even when the change is unrelated just in case we missed a previous
            // update
            checkConfigAndMaybeDownloadModels();
          }
        };
  }

  /** Register a listener to related DeviceConfig flag changes. */
  public void init() {
    DeviceConfig.addOnPropertiesChangedListener(
        DeviceConfig.NAMESPACE_TEXTCLASSIFIER, listenerExecutor, deviceConfigListener);
    TcLog.d(TAG, "DeviceConfig listener registered by ModelDownloadManager");
    // Check flags in case any updates heppened before the TCS starts
    checkConfigAndMaybeDownloadModels();
  }

  /** Un-register the listener to DeviceConfig. */
  public void destroy() {
    DeviceConfig.removeOnPropertiesChangedListener(deviceConfigListener);
    TcLog.d(TAG, "DeviceConfig listener unregistered by ModelDownloadeManager");
  }

  /**
   * Check DeviceConfig and issue new model downloaing requests if needed. This is the entry point
   * to the whole downloading logic.
   */
  private void checkConfigAndMaybeDownloadModels() {
    TcLog.d(TAG, "Checking DeviceConfig to see whether there are new models to download");
    // TODO(licha): implement this methods with Downlaoder2 lib, WorkManager and androidx.room
  }
}
