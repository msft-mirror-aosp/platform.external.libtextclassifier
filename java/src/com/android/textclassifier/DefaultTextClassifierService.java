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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.CancellationSignal;
import android.service.textclassifier.TextClassifierService;
import android.view.textclassifier.ConversationActions;
import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassificationSessionId;
import android.view.textclassifier.TextClassifierEvent;
import android.view.textclassifier.TextLanguage;
import android.view.textclassifier.TextLinks;
import android.view.textclassifier.TextSelection;
import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.common.statsd.TextClassifierApiUsageLogger;
import com.android.textclassifier.utils.IndentingPrintWriter;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.ListeningExecutorService;
import com.google.common.util.concurrent.MoreExecutors;
import com.google.common.util.concurrent.ThreadFactoryBuilder;
import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.concurrent.Callable;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/** An implementation of a TextClassifierService. */
public final class DefaultTextClassifierService extends TextClassifierService {
  private static final String TAG = "default_tcs";

  private final Injector injector;
  // TODO: Figure out do we need more concurrency.
  private ListeningExecutorService normPriorityExecutor;
  private ListeningExecutorService lowPriorityExecutor;
  private TextClassifierImpl textClassifier;
  private TextClassifierSettings settings;
  private ModelFileManager modelFileManager;
  private BroadcastReceiver localeChangedReceiver;

  public DefaultTextClassifierService() {
    this.injector = new InjectorImpl(this);
  }

  @VisibleForTesting
  DefaultTextClassifierService(Injector injector) {
    this.injector = Preconditions.checkNotNull(injector);
  }

  private TextClassifierApiUsageLogger textClassifierApiUsageLogger;

  @Override
  public void onCreate() {
    super.onCreate();

    settings = injector.createTextClassifierSettings();
    modelFileManager = injector.createModelFileManager(settings);
    normPriorityExecutor = injector.createNormPriorityExecutor();
    lowPriorityExecutor = injector.createLowPriorityExecutor();
    textClassifier = injector.createTextClassifierImpl(settings, modelFileManager);
    localeChangedReceiver = new LocaleChangedReceiver(modelFileManager);

    textClassifierApiUsageLogger =
        injector.createTextClassifierApiUsageLogger(settings, lowPriorityExecutor);

    injector
        .getContext()
        .registerReceiver(localeChangedReceiver, new IntentFilter(Intent.ACTION_LOCALE_CHANGED));
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    injector.getContext().unregisterReceiver(localeChangedReceiver);
  }

  @Override
  public void onSuggestSelection(
      TextClassificationSessionId sessionId,
      TextSelection.Request request,
      CancellationSignal cancellationSignal,
      Callback<TextSelection> callback) {
    handleRequestAsync(
        () -> textClassifier.suggestSelection(request),
        callback,
        textClassifierApiUsageLogger.createSession(
            TextClassifierApiUsageLogger.API_TYPE_SUGGEST_SELECTION),
        cancellationSignal);
  }

  @Override
  public void onClassifyText(
      TextClassificationSessionId sessionId,
      TextClassification.Request request,
      CancellationSignal cancellationSignal,
      Callback<TextClassification> callback) {
    handleRequestAsync(
        () -> textClassifier.classifyText(request),
        callback,
        textClassifierApiUsageLogger.createSession(
            TextClassifierApiUsageLogger.API_TYPE_CLASSIFY_TEXT),
        cancellationSignal);
  }

  @Override
  public void onGenerateLinks(
      TextClassificationSessionId sessionId,
      TextLinks.Request request,
      CancellationSignal cancellationSignal,
      Callback<TextLinks> callback) {
    handleRequestAsync(
        () -> textClassifier.generateLinks(request),
        callback,
        textClassifierApiUsageLogger.createSession(
            TextClassifierApiUsageLogger.API_TYPE_GENERATE_LINKS),
        cancellationSignal);
  }

  @Override
  public void onSuggestConversationActions(
      TextClassificationSessionId sessionId,
      ConversationActions.Request request,
      CancellationSignal cancellationSignal,
      Callback<ConversationActions> callback) {
    handleRequestAsync(
        () -> textClassifier.suggestConversationActions(request),
        callback,
        textClassifierApiUsageLogger.createSession(
            TextClassifierApiUsageLogger.API_TYPE_SUGGEST_CONVERSATION_ACTIONS),
        cancellationSignal);
  }

  @Override
  public void onDetectLanguage(
      TextClassificationSessionId sessionId,
      TextLanguage.Request request,
      CancellationSignal cancellationSignal,
      Callback<TextLanguage> callback) {
    handleRequestAsync(
        () -> textClassifier.detectLanguage(request),
        callback,
        textClassifierApiUsageLogger.createSession(
            TextClassifierApiUsageLogger.API_TYPE_DETECT_LANGUAGES),
        cancellationSignal);
  }

  @Override
  public void onSelectionEvent(TextClassificationSessionId sessionId, SelectionEvent event) {
    handleEvent(() -> textClassifier.onSelectionEvent(event));
  }

  @Override
  public void onTextClassifierEvent(
      TextClassificationSessionId sessionId, TextClassifierEvent event) {
    handleEvent(() -> textClassifier.onTextClassifierEvent(sessionId, event));
  }

  @Override
  protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
    IndentingPrintWriter indentingPrintWriter = new IndentingPrintWriter(writer);
    textClassifier.dump(indentingPrintWriter);
    indentingPrintWriter.flush();
  }

  private <T> void handleRequestAsync(
      Callable<T> callable,
      Callback<T> callback,
      TextClassifierApiUsageLogger.Session apiLoggerSession,
      CancellationSignal cancellationSignal) {
    ListenableFuture<T> result = normPriorityExecutor.submit(callable);
    Futures.addCallback(
        result,
        new FutureCallback<T>() {
          @Override
          public void onSuccess(T result) {
            callback.onSuccess(result);
            apiLoggerSession.reportSuccess();
          }

          @Override
          public void onFailure(Throwable t) {
            TcLog.e(TAG, "onFailure: ", t);
            callback.onFailure(t.getMessage());
            apiLoggerSession.reportFailure();
          }
        },
        MoreExecutors.directExecutor());
    cancellationSignal.setOnCancelListener(() -> result.cancel(/* mayInterruptIfRunning= */ true));
  }

  private void handleEvent(Runnable runnable) {
    ListenableFuture<Void> result =
        lowPriorityExecutor.submit(
            () -> {
              runnable.run();
              return null;
            });
    Futures.addCallback(
        result,
        new FutureCallback<Void>() {
          @Override
          public void onSuccess(Void result) {}

          @Override
          public void onFailure(Throwable t) {
            TcLog.e(TAG, "onFailure: ", t);
          }
        },
        MoreExecutors.directExecutor());
  }

  /**
   * Receiver listening to locale change event. Ask ModelFileManager to do clean-up upon receiving.
   */
  static class LocaleChangedReceiver extends BroadcastReceiver {
    private final ModelFileManager modelFileManager;

    LocaleChangedReceiver(ModelFileManager modelFileManager) {
      this.modelFileManager = modelFileManager;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
      modelFileManager.deleteUnusedModelFiles();
    }
  }

  // Do not call any of these methods, except the constructor, before Service.onCreate is called.
  private static class InjectorImpl implements Injector {
    // Do not access the context object before Service.onCreate is invoked.
    private final Context context;

    private InjectorImpl(Context context) {
      this.context = Preconditions.checkNotNull(context);
    }

    @Override
    public Context getContext() {
      return context;
    }

    @Override
    public ModelFileManager createModelFileManager(TextClassifierSettings settings) {
      return new ModelFileManager(context, settings);
    }

    @Override
    public TextClassifierSettings createTextClassifierSettings() {
      return new TextClassifierSettings();
    }

    @Override
    public TextClassifierImpl createTextClassifierImpl(
        TextClassifierSettings settings, ModelFileManager modelFileManager) {
      return new TextClassifierImpl(context, settings, modelFileManager);
    }

    @Override
    public ListeningExecutorService createNormPriorityExecutor() {
      return MoreExecutors.listeningDecorator(
          Executors.newFixedThreadPool(
              /* nThreads= */ 2,
              new ThreadFactoryBuilder()
                  .setNameFormat("tcs-norm-prio-executor")
                  .setPriority(Thread.NORM_PRIORITY)
                  .build()));
    }

    @Override
    public ListeningExecutorService createLowPriorityExecutor() {
      return MoreExecutors.listeningDecorator(
          Executors.newSingleThreadExecutor(
              new ThreadFactoryBuilder()
                  .setNameFormat("tcs-low-prio-executor")
                  .setPriority(Thread.NORM_PRIORITY - 1)
                  .build()));
    }

    @Override
    public TextClassifierApiUsageLogger createTextClassifierApiUsageLogger(
        TextClassifierSettings settings, Executor executor) {
      return new TextClassifierApiUsageLogger(
          settings::getTextClassifierApiLogSampleRate, executor);
    }
  }

  /*
   * Provides dependencies to the {@link DefaultTextClassifierService}. This makes the service
   * class testable.
   */
  interface Injector {
    Context getContext();

    ModelFileManager createModelFileManager(TextClassifierSettings settings);

    TextClassifierSettings createTextClassifierSettings();

    TextClassifierImpl createTextClassifierImpl(
        TextClassifierSettings settings, ModelFileManager modelFileManager);

    ListeningExecutorService createNormPriorityExecutor();

    ListeningExecutorService createLowPriorityExecutor();

    TextClassifierApiUsageLogger createTextClassifierApiUsageLogger(
        TextClassifierSettings settings, Executor executor);
  }
}
