/*
 * Copyright (C) 2019 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.textclassifier;

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

import com.android.textclassifier.utils.IndentingPrintWriter;

import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.ListeningExecutorService;
import com.google.common.util.concurrent.MoreExecutors;
import com.google.common.util.concurrent.ThreadFactoryBuilder;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.concurrent.Callable;
import java.util.concurrent.Executors;

public final class DefaultTextClassifierService extends TextClassifierService {
    private static final String TAG = "default_tcs";

    // TODO: Figure out do we need more concurrency.
    private final ListeningExecutorService mNormPriorityExecutor =
            MoreExecutors.listeningDecorator(
                    Executors.newFixedThreadPool(
                            /* nThreads= */ 2,
                            new ThreadFactoryBuilder()
                                    .setNameFormat("tcs-norm-prio-executor")
                                    .setPriority(Thread.NORM_PRIORITY)
                                    .build()));

    private final ListeningExecutorService mLowPriorityExecutor =
            MoreExecutors.listeningDecorator(
                    Executors.newSingleThreadExecutor(
                            new ThreadFactoryBuilder()
                                    .setNameFormat("tcs-low-prio-executor")
                                    .setPriority(Thread.NORM_PRIORITY - 1)
                                    .build()));

    private TextClassifierImpl mTextClassifier;

    @Override
    public void onCreate() {
        super.onCreate();
        mTextClassifier = new TextClassifierImpl(this, new TextClassificationConstants());
    }

    @Override
    public void onSuggestSelection(
            TextClassificationSessionId sessionId,
            TextSelection.Request request,
            CancellationSignal cancellationSignal,
            Callback<TextSelection> callback) {
        handleRequestAsync(
                () -> mTextClassifier.suggestSelection(request), callback, cancellationSignal);
    }

    @Override
    public void onClassifyText(
            TextClassificationSessionId sessionId,
            TextClassification.Request request,
            CancellationSignal cancellationSignal,
            Callback<TextClassification> callback) {
        handleRequestAsync(
                () -> mTextClassifier.classifyText(request), callback, cancellationSignal);
    }

    @Override
    public void onGenerateLinks(
            TextClassificationSessionId sessionId,
            TextLinks.Request request,
            CancellationSignal cancellationSignal,
            Callback<TextLinks> callback) {
        handleRequestAsync(
                () -> mTextClassifier.generateLinks(request), callback, cancellationSignal);
    }

    @Override
    public void onSuggestConversationActions(
            TextClassificationSessionId sessionId,
            ConversationActions.Request request,
            CancellationSignal cancellationSignal,
            Callback<ConversationActions> callback) {
        handleRequestAsync(
                () -> mTextClassifier.suggestConversationActions(request),
                callback,
                cancellationSignal);
    }

    @Override
    public void onDetectLanguage(
            TextClassificationSessionId sessionId,
            TextLanguage.Request request,
            CancellationSignal cancellationSignal,
            Callback<TextLanguage> callback) {
        handleRequestAsync(
                () -> mTextClassifier.detectLanguage(request), callback, cancellationSignal);
    }

    @Override
    public void onSelectionEvent(TextClassificationSessionId sessionId, SelectionEvent event) {
        handleEvent(() -> mTextClassifier.onSelectionEvent(event));
    }

    @Override
    public void onTextClassifierEvent(
            TextClassificationSessionId sessionId, TextClassifierEvent event) {
        handleEvent(() -> mTextClassifier.onTextClassifierEvent(sessionId, event));
    }

    @Override
    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        IndentingPrintWriter indentingPrintWriter = new IndentingPrintWriter(writer);
        mTextClassifier.dump(indentingPrintWriter);
        indentingPrintWriter.flush();
    }

    private <T> void handleRequestAsync(
            Callable<T> callable, Callback<T> callback, CancellationSignal cancellationSignal) {
        ListenableFuture<T> result = mNormPriorityExecutor.submit(callable);
        Futures.addCallback(
                result,
                new FutureCallback<T>() {
                    @Override
                    public void onSuccess(T result) {
                        callback.onSuccess(result);
                    }

                    @Override
                    public void onFailure(Throwable t) {
                        TcLog.e(TAG, "onFailure: ", t);
                        callback.onFailure(t.getMessage());
                    }
                },
                MoreExecutors.directExecutor());
        cancellationSignal.setOnCancelListener(
                () -> result.cancel(/* mayInterruptIfRunning= */ true));
    }

    private void handleEvent(Runnable runnable) {
        ListenableFuture<Void> result =
                mLowPriorityExecutor.submit(
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
}
