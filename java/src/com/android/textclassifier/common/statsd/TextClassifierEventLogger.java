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

package com.android.textclassifier.common.statsd;

import android.view.textclassifier.TextClassifier;
import com.android.textclassifier.TextClassifierStatsLog;
import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.common.logging.TextClassificationContext;
import com.android.textclassifier.common.logging.TextClassificationSessionId;
import com.android.textclassifier.common.logging.TextClassifierEvent;
import com.google.common.base.Preconditions;
import javax.annotation.Nullable;

/** Logs {@link android.view.textclassifier.TextClassifierEvent}. */
public final class TextClassifierEventLogger {

  private static final String TAG = "TCEventLogger";

  /** Emits a text classifier event to the logs. */
  public void writeEvent(
      @Nullable TextClassificationSessionId sessionId, TextClassifierEvent event) {
    Preconditions.checkNotNull(event);
    if (TcLog.ENABLE_FULL_LOGGING) {
      TcLog.v(TAG, "TextClassifierEventLogger.writeEvent: event = [" + event + "]");
    }
    if (event instanceof TextClassifierEvent.TextSelectionEvent) {
      logTextSelectionEvent(sessionId, (TextClassifierEvent.TextSelectionEvent) event);
    } else if (event instanceof TextClassifierEvent.TextLinkifyEvent) {
      logTextLinkifyEvent(sessionId, (TextClassifierEvent.TextLinkifyEvent) event);
    } else if (event instanceof TextClassifierEvent.ConversationActionsEvent) {
      logConversationActionsEvent(sessionId, (TextClassifierEvent.ConversationActionsEvent) event);
    } else if (event instanceof TextClassifierEvent.LanguageDetectionEvent) {
      logLanguageDetectionEvent(sessionId, (TextClassifierEvent.LanguageDetectionEvent) event);
    } else {
      TcLog.w(TAG, "Unexpected events, category=" + event.getEventCategory());
    }
  }

  private static void logTextSelectionEvent(
      @Nullable TextClassificationSessionId sessionId,
      TextClassifierEvent.TextSelectionEvent event) {
    TextClassifierStatsLog.write(
        TextClassifierStatsLog.TEXT_SELECTION_EVENT,
        sessionId == null ? null : sessionId.flattenToString(),
        event.getEventType(),
        getModelName(event),
        getWidgetType(event),
        event.getEventIndex(),
        getItemAt(event.getEntityTypes(), /* index= */ 0),
        event.getRelativeWordStartIndex(),
        event.getRelativeWordEndIndex(),
        event.getRelativeSuggestedWordStartIndex(),
        event.getRelativeSuggestedWordEndIndex(),
        getPackageName(event));
  }

  private static void logTextLinkifyEvent(
      TextClassificationSessionId sessionId, TextClassifierEvent.TextLinkifyEvent event) {
    TextClassifierStatsLog.write(
        TextClassifierStatsLog.TEXT_LINKIFY_EVENT,
        sessionId == null ? null : sessionId.flattenToString(),
        event.getEventType(),
        getModelName(event),
        getWidgetType(event),
        event.getEventIndex(),
        getItemAt(event.getEntityTypes(), /* index= */ 0),
        /*numOfLinks=*/ 0,
        /*linkedTextLength=*/ 0,
        /*textLength=*/ 0,
        /*latencyInMillis=*/ 0L,
        getPackageName(event));
  }

  private static void logConversationActionsEvent(
      @Nullable TextClassificationSessionId sessionId,
      TextClassifierEvent.ConversationActionsEvent event) {
    TextClassifierStatsLog.write(
        TextClassifierStatsLog.CONVERSATION_ACTIONS_EVENT,
        sessionId == null
            ? event.getResultId() // TODO: Update ExtServices to set the session id.
            : sessionId.flattenToString(),
        event.getEventType(),
        getModelName(event),
        getWidgetType(event),
        getItemAt(event.getEntityTypes(), /* index= */ 0),
        getItemAt(event.getEntityTypes(), /* index= */ 1),
        getItemAt(event.getEntityTypes(), /* index= */ 2),
        getFloatAt(event.getScores(), /* index= */ 0),
        getPackageName(event),
        "");
  }

  private static void logLanguageDetectionEvent(
      @Nullable TextClassificationSessionId sessionId,
      TextClassifierEvent.LanguageDetectionEvent event) {
    TextClassifierStatsLog.write(
        TextClassifierStatsLog.LANGUAGE_DETECTION_EVENT,
        sessionId == null ? null : sessionId.flattenToString(),
        event.getEventType(),
        getModelName(event),
        getWidgetType(event),
        getItemAt(event.getEntityTypes(), /* index= */ 0),
        getFloatAt(event.getScores(), /* index= */ 0),
        getIntAt(event.getActionIndices(), /* index= */ 0),
        getPackageName(event));
  }

  @Nullable
  private static <T> T getItemAt(@Nullable T[] array, int index) {
    if (array == null) {
      return null;
    }
    if (index >= array.length) {
      return null;
    }
    return array[index];
  }

  private static float getFloatAt(@Nullable float[] array, int index) {
    if (array == null) {
      return 0f;
    }
    if (index >= array.length) {
      return 0f;
    }
    return array[index];
  }

  private static int getIntAt(@Nullable int[] array, int index) {
    if (array == null) {
      return 0;
    }
    if (index >= array.length) {
      return 0;
    }
    return array[index];
  }

  private static String getModelName(TextClassifierEvent event) {
    if (event.getModelName() != null) {
      return event.getModelName();
    }
    return ResultIdUtils.getModelName(event.getResultId());
  }

  @Nullable
  private static String getPackageName(TextClassifierEvent event) {
    TextClassificationContext eventContext = event.getEventContext();
    if (eventContext == null) {
      return null;
    }
    return eventContext.getPackageName();
  }

  private static int getWidgetType(TextClassifierEvent event) {
    TextClassificationContext eventContext = event.getEventContext();
    if (eventContext == null) {
      return WidgetType.WIDGET_TYPE_UNKNOWN;
    }
    switch (eventContext.getWidgetType()) {
      case TextClassifier.WIDGET_TYPE_UNKNOWN:
        return WidgetType.WIDGET_TYPE_UNKNOWN;
      case TextClassifier.WIDGET_TYPE_TEXTVIEW:
        return WidgetType.WIDGET_TYPE_TEXTVIEW;
      case TextClassifier.WIDGET_TYPE_EDITTEXT:
        return WidgetType.WIDGET_TYPE_EDITTEXT;
      case TextClassifier.WIDGET_TYPE_UNSELECTABLE_TEXTVIEW:
        return WidgetType.WIDGET_TYPE_UNSELECTABLE_TEXTVIEW;
      case TextClassifier.WIDGET_TYPE_WEBVIEW:
        return WidgetType.WIDGET_TYPE_WEBVIEW;
      case TextClassifier.WIDGET_TYPE_EDIT_WEBVIEW:
        return WidgetType.WIDGET_TYPE_EDIT_WEBVIEW;
      case TextClassifier.WIDGET_TYPE_CUSTOM_TEXTVIEW:
        return WidgetType.WIDGET_TYPE_CUSTOM_TEXTVIEW;
      case TextClassifier.WIDGET_TYPE_CUSTOM_EDITTEXT:
        return WidgetType.WIDGET_TYPE_CUSTOM_EDITTEXT;
      case TextClassifier.WIDGET_TYPE_CUSTOM_UNSELECTABLE_TEXTVIEW:
        return WidgetType.WIDGET_TYPE_CUSTOM_UNSELECTABLE_TEXTVIEW;
      case TextClassifier.WIDGET_TYPE_NOTIFICATION:
        return WidgetType.WIDGET_TYPE_NOTIFICATION;
      default: // fall out
    }
    return WidgetType.WIDGET_TYPE_UNKNOWN;
  }

  /** Widget type constants for logging. */
  public static final class WidgetType {
    // Sync these constants with textclassifier_enums.proto.
    public static final int WIDGET_TYPE_UNKNOWN = 0;
    public static final int WIDGET_TYPE_TEXTVIEW = 1;
    public static final int WIDGET_TYPE_EDITTEXT = 2;
    public static final int WIDGET_TYPE_UNSELECTABLE_TEXTVIEW = 3;
    public static final int WIDGET_TYPE_WEBVIEW = 4;
    public static final int WIDGET_TYPE_EDIT_WEBVIEW = 5;
    public static final int WIDGET_TYPE_CUSTOM_TEXTVIEW = 6;
    public static final int WIDGET_TYPE_CUSTOM_EDITTEXT = 7;
    public static final int WIDGET_TYPE_CUSTOM_UNSELECTABLE_TEXTVIEW = 8;
    public static final int WIDGET_TYPE_NOTIFICATION = 9;

    private WidgetType() {}
  }
}
