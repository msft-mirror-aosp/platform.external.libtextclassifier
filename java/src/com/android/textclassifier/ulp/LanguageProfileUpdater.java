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

package com.android.textclassifier.ulp;

import android.content.Context;
import android.util.LruCache;
import android.view.textclassifier.ConversationActions;
import com.android.textclassifier.TcLog;
import com.android.textclassifier.ulp.database.LanguageProfileDatabase;
import com.android.textclassifier.ulp.database.LanguageSignalInfo;
import com.android.textclassifier.utils.IndentingPrintWriter;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.ListeningExecutorService;
import com.google.common.util.concurrent.MoreExecutors;
import com.google.errorprone.annotations.CanIgnoreReturnValue;
import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.function.Function;
import javax.annotation.Nullable;

/** Class implementing functions which builds and updates user language profile. */
public class LanguageProfileUpdater {
  private static final String TAG = "LanguageProfileUpdater";
  private static final int MAX_CACHE_SIZE = 20;
  private static final String DEFAULT_NOTIFICATION_KEY = "DEFAULT_KEY";

  static final String NOTIFICATION_KEY = "notificationKey";

  private final LanguageProfileDatabase languageProfileDatabase;
  private final ListeningExecutorService executorService;
  private final LruCache<String, Long> updatedNotifications = new LruCache<>(MAX_CACHE_SIZE);

  public LanguageProfileUpdater(Context context, ListeningExecutorService executorService) {
    languageProfileDatabase = LanguageProfileDatabase.getInstance(context);
    this.executorService = executorService;
  }

  @VisibleForTesting
  LanguageProfileUpdater(
      ListeningExecutorService executorService, LanguageProfileDatabase database) {
    languageProfileDatabase = database;
    this.executorService = executorService;
  }

  /** Updates counts of languages found in suggestConversationActions. */
  @CanIgnoreReturnValue
  public ListenableFuture<Void> updateFromConversationActionsAsync(
      ConversationActions.Request request, Function<CharSequence, List<String>> languageDetector) {
    return runAsync(
        () -> {
          ConversationActions.Message msg = getMessageFromRequest(request);
          if (msg == null) {
            return null;
          }
          List<String> languageTags = languageDetector.apply(msg.getText().toString());
          String notificationKey =
              request.getExtras().getString(NOTIFICATION_KEY, DEFAULT_NOTIFICATION_KEY);
          Long messageReferenceTime = getMessageReferenceTime(msg);
          if (isNewMessage(notificationKey, messageReferenceTime)) {
            for (String tag : languageTags) {
              increaseSignalCountInDatabase(
                  tag, LanguageSignalInfo.SUGGEST_CONVERSATION_ACTIONS, 1);
            }
          }
          return null;
        });
  }

  /** Updates counts of languages found in classifyText. */
  @CanIgnoreReturnValue
  public ListenableFuture<Void> updateFromClassifyTextAsync(List<String> detectedLanguageTags) {
    return runAsync(
        () -> {
          for (String languageTag : detectedLanguageTags) {
            increaseSignalCountInDatabase(
                languageTag, LanguageSignalInfo.CLASSIFY_TEXT, /* increment= */ 1);
          }
          return null;
        });
  }

  /** Runs the specified callable asynchronously and prints the stack trace if it failed. */
  private <T> ListenableFuture<T> runAsync(Callable<T> callable) {
    ListenableFuture<T> future = executorService.submit(callable);
    Futures.addCallback(
        future,
        new FutureCallback<T>() {
          @Override
          public void onSuccess(T result) {}

          @Override
          public void onFailure(Throwable t) {
            TcLog.e(TAG, "runAsync", t);
          }
        },
        MoreExecutors.directExecutor());
    return future;
  }

  private void increaseSignalCountInDatabase(
      String languageTag, @LanguageSignalInfo.Source int sourceType, int increment) {
    languageProfileDatabase
        .languageInfoDao()
        .increaseSignalCount(languageTag, sourceType, increment);
  }

  @Nullable
  private static ConversationActions.Message getMessageFromRequest(
      ConversationActions.Request request) {
    int size = request.getConversation().size();
    if (size == 0) {
      return null;
    }
    return request.getConversation().get(size - 1);
  }

  private boolean isNewMessage(String notificationKey, Long sendTime) {
    Long oldTime = updatedNotifications.get(notificationKey);

    if (oldTime == null || sendTime > oldTime) {
      updatedNotifications.put(notificationKey, sendTime);
      return true;
    }
    return false;
  }

  private static long getMessageReferenceTime(ConversationActions.Message msg) {
    return msg.getReferenceTime() == null
        ? Instant.now().toEpochMilli()
        : msg.getReferenceTime().toInstant().toEpochMilli();
  }

  /** Dumps the data on the screen when called. */
  public void dump(IndentingPrintWriter printWriter) {
    printWriter.println("LanguageProfileUpdater:");
    printWriter.increaseIndent();
    printWriter.println("Cache for notifications status:");
    printWriter.increaseIndent();
    for (Map.Entry<String, Long> entry : updatedNotifications.snapshot().entrySet()) {
      long timestamp = entry.getValue();
      printWriter.println(
          "Notification key: "
              + entry.getKey()
              + " time: "
              + timestamp
              + " ("
              + Instant.ofEpochMilli(timestamp).toString()
              + ")");
    }
    printWriter.decreaseIndent();
    printWriter.decreaseIndent();
  }
}
