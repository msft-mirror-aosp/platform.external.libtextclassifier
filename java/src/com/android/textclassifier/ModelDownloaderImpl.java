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

import static android.content.Context.BIND_AUTO_CREATE;
import static android.content.Context.BIND_NOT_FOREGROUND;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import androidx.concurrent.futures.CallbackToFutureAdapter;
import com.android.textclassifier.common.base.TcLog;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import java.io.File;
import java.net.URI;
import java.util.concurrent.ExecutorService;

/**
 * ModelDownloader implementation that forwards requests to ModelDownloaderService. This is to
 * restrict the INTERNET permission to the service process only (instead of the whole ExtServices).
 */
final class ModelDownloaderImpl implements ModelDownloader {
  private static final String TAG = "ModelDownloaderImpl";

  private final Context context;
  private final ExecutorService bgExecutorService;
  private final Class<?> downloaderServiceClass;

  public ModelDownloaderImpl(Context context, ExecutorService bgExecutorService) {
    this(context, bgExecutorService, ModelDownloaderService.class);
  }

  @VisibleForTesting
  ModelDownloaderImpl(
      Context context, ExecutorService bgExecutorService, Class<?> downloaderServiceClass) {
    this.context = context.getApplicationContext();
    this.bgExecutorService = bgExecutorService;
    this.downloaderServiceClass = downloaderServiceClass;
  }

  @Override
  public ListenableFuture<Long> download(URI uri, File targetFile) {
    DownloaderServiceConnection conn = new DownloaderServiceConnection();
    ListenableFuture<IModelDownloaderService> downloaderServiceFuture = connect(conn);
    ListenableFuture<Long> bytesWrittenFuture =
        Futures.transformAsync(
            downloaderServiceFuture,
            service -> scheduleDownload(service, uri, targetFile),
            bgExecutorService);
    bytesWrittenFuture.addListener(
        () -> {
          try {
            context.unbindService(conn);
          } catch (IllegalArgumentException e) {
            TcLog.e(TAG, "Error when unbind", e);
          }
        },
        bgExecutorService);
    return bytesWrittenFuture;
  }

  private ListenableFuture<IModelDownloaderService> connect(DownloaderServiceConnection conn) {
    TcLog.d(TAG, "Starting a new connection to ModelDownloaderService");
    return CallbackToFutureAdapter.getFuture(
        completer -> {
          conn.attachCompleter(completer);
          Intent intent = new Intent(context, downloaderServiceClass);
          if (context.bindService(intent, conn, BIND_AUTO_CREATE | BIND_NOT_FOREGROUND)) {
            return "Binding to service";
          } else {
            completer.setException(new RuntimeException("Unable to bind to service"));
            return "Binding failed";
          }
        });
  }

  // Here the returned download result future can be set by: 1) the service can invoke the callback
  // and set the result/exception; 2) If the service crashed, the CallbackToFutureAdapter will try
  // to fail the future when the callback is garbage collected. If somehow none of them worked, the
  // restult future will hang there until time out. (WorkManager forces a 10-min running time.)
  private static ListenableFuture<Long> scheduleDownload(
      IModelDownloaderService service, URI uri, File targetFile) {
    TcLog.d(TAG, "Scheduling a new download task with ModelDownloaderService");
    return CallbackToFutureAdapter.getFuture(
        completer -> {
          service.download(
              uri.toString(),
              targetFile.getAbsolutePath(),
              new IModelDownloaderCallback.Stub() {
                @Override
                public void onSuccess(long bytesWritten) {
                  completer.set(bytesWritten);
                }

                @Override
                public void onFailure(String errorMsg) {
                  completer.setException(new RuntimeException(errorMsg));
                }
              });
          return "downlaoderService.download";
        });
  }

  /** The implementation of {@link ServiceConnection} that handles changes in the connection. */
  @VisibleForTesting
  static class DownloaderServiceConnection implements ServiceConnection {
    private static final String TAG = "ModelDownloaderImpl.DownloaderServiceConnection";

    private CallbackToFutureAdapter.Completer<IModelDownloaderService> completer;

    public void attachCompleter(
        CallbackToFutureAdapter.Completer<IModelDownloaderService> completer) {
      this.completer = completer;
    }

    @Override
    public void onServiceConnected(ComponentName componentName, IBinder iBinder) {
      TcLog.d(TAG, "DownloaderService connected");
      completer.set(Preconditions.checkNotNull(IModelDownloaderService.Stub.asInterface(iBinder)));
    }

    @Override
    public void onServiceDisconnected(ComponentName componentName) {
      // If this is invoked after onServiceConnected, it will be ignored by the completer.
      completer.setException(new RuntimeException("Service disconnected"));
    }

    @Override
    public void onBindingDied(ComponentName name) {
      completer.setException(new RuntimeException("Binding died"));
    }

    @Override
    public void onNullBinding(ComponentName name) {
      completer.setException(new RuntimeException("Unable to bind to DownloaderService"));
    }
  }
}
