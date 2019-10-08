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
 * limitations under the License.
 */

package com.android.textclassifier.logging;

import static com.google.common.truth.Truth.assertThat;

import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassificationSessionId;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;

import com.android.os.AtomsProto;
import com.android.textclassifier.TextClassifierStatsLog;
import com.android.textclassifier.shadows.ShadowTextClassifierStatsLog;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = ShadowTextClassifierStatsLog.class)
public class TextClassifierEventLoggerTest {
    private static final String PKG_NAME = "pkg.name";
    private static final String WIDGET_TYPE = TextClassifier.WIDGET_TYPE_WEBVIEW;

    private TextClassifierEventLogger mTextClassifierEventLogger;

    @Before
    public void setup() {
        mTextClassifierEventLogger = new TextClassifierEventLogger();
    }

    @After
    public void teardown() {
        ShadowTextClassifierStatsLog.reset();
    }

    @Test
    public void writeEvent_textSelectionEvent() {
        TextClassificationSessionId sessionId = new TextClassificationSessionId();
        TextClassifierEvent.TextSelectionEvent textSelectionEvent =
                new TextClassifierEvent.TextSelectionEvent.Builder(
                                TextClassifierEvent.TYPE_SELECTION_STARTED)
                        .setEventContext(createTextClassificationContext())
                        .setModelName("model_name")
                        .setEventIndex(1)
                        .setEntityTypes(TextClassifier.TYPE_ADDRESS)
                        .setRelativeWordStartIndex(2)
                        .setRelativeWordEndIndex(3)
                        .setRelativeSuggestedWordStartIndex(1)
                        .setRelativeSuggestedWordEndIndex(4)
                        .build();

        mTextClassifierEventLogger.writeEvent(sessionId, textSelectionEvent);
        ShadowTextClassifierStatsLog.LoggedEvent loggedEvent =
                ShadowTextClassifierStatsLog.getLoggedEvents().get(0);
        AtomsProto.TextSelectionEvent protoEvent =
                (AtomsProto.TextSelectionEvent) loggedEvent.event;

        assertThat(loggedEvent.code).isEqualTo(TextClassifierStatsLog.TEXT_SELECTION_EVENT);
        assertThat(protoEvent.getSessionId()).isEqualTo(sessionId.flattenToString());
        assertThat(protoEvent.getEventType().getNumber())
                .isEqualTo(TextClassifierEvent.TYPE_SELECTION_STARTED);
        assertThat(protoEvent.getSessionId()).isEqualTo(sessionId.flattenToString());
        assertThat(protoEvent.getModelName()).isEqualTo("model_name");
        assertThat(protoEvent.getWidgetType().getNumber())
                .isEqualTo(TextClassifierEventLogger.WidgetType.WIDGET_TYPE_WEBVIEW);
        assertThat(protoEvent.getEventIndex()).isEqualTo(1);
        assertThat(protoEvent.getEntityType()).isEqualTo(TextClassifier.TYPE_ADDRESS);
        assertThat(protoEvent.getRelativeWordStartIndex()).isEqualTo(2);
        assertThat(protoEvent.getRelativeWordEndIndex()).isEqualTo(3);
        assertThat(protoEvent.getRelativeSuggestedWordStartIndex()).isEqualTo(1);
        assertThat(protoEvent.getRelativeSuggestedWordEndIndex()).isEqualTo(4);
        assertThat(protoEvent.getPackageName()).isEqualTo(PKG_NAME);
    }

    @Test
    public void writeEvent_textLinkifyEvent() {
        TextClassificationSessionId sessionId = new TextClassificationSessionId();
        TextClassifierEvent.TextLinkifyEvent textLinkifyEvent =
                new TextClassifierEvent.TextLinkifyEvent.Builder(
                                TextClassifierEvent.TYPE_SELECTION_STARTED)
                        .setEventContext(createTextClassificationContext())
                        .setModelName("model_name")
                        .setEventIndex(1)
                        .setEntityTypes(TextClassifier.TYPE_ADDRESS)
                        .build();

        mTextClassifierEventLogger.writeEvent(sessionId, textLinkifyEvent);
        ShadowTextClassifierStatsLog.LoggedEvent loggedEvent =
                ShadowTextClassifierStatsLog.getLoggedEvents().get(0);
        AtomsProto.TextLinkifyEvent protoEvent = (AtomsProto.TextLinkifyEvent) loggedEvent.event;

        assertThat(loggedEvent.code).isEqualTo(TextClassifierStatsLog.TEXT_LINKIFY_EVENT);
        assertThat(protoEvent.getSessionId()).isEqualTo(sessionId.flattenToString());
        assertThat(protoEvent.getEventType().getNumber())
                .isEqualTo(TextClassifierEvent.TYPE_SELECTION_STARTED);
        assertThat(protoEvent.getModelName()).isEqualTo("model_name");
        assertThat(protoEvent.getWidgetType().getNumber())
                .isEqualTo(TextClassifierEventLogger.WidgetType.WIDGET_TYPE_WEBVIEW);
        assertThat(protoEvent.getEventIndex()).isEqualTo(1);
        assertThat(protoEvent.getEntityType()).isEqualTo(TextClassifier.TYPE_ADDRESS);
        assertThat(protoEvent.getNumLinks()).isEqualTo(0);
        assertThat(protoEvent.getLinkedTextLength()).isEqualTo(0);
        assertThat(protoEvent.getTextLength()).isEqualTo(0);
        assertThat(protoEvent.getLatencyMillis()).isEqualTo(0);
        assertThat(protoEvent.getPackageName()).isEqualTo(PKG_NAME);
    }

    @Test
    public void writeEvent_conversationActionsEvent() {
        TextClassificationSessionId sessionId = new TextClassificationSessionId();
        TextClassifierEvent.ConversationActionsEvent conversationActionsEvent =
                new TextClassifierEvent.ConversationActionsEvent.Builder(
                                TextClassifierEvent.TYPE_SELECTION_STARTED)
                        .setEventContext(createTextClassificationContext())
                        .setModelName("model_name")
                        .setEventIndex(1)
                        .setEntityTypes("first", "second", "third", "fourth")
                        .setScores(0.5f)
                        .build();

        mTextClassifierEventLogger.writeEvent(sessionId, conversationActionsEvent);
        ShadowTextClassifierStatsLog.LoggedEvent loggedEvent =
                ShadowTextClassifierStatsLog.getLoggedEvents().get(0);
        AtomsProto.ConversationActionsEvent protoEvent =
                (AtomsProto.ConversationActionsEvent) loggedEvent.event;

        assertThat(loggedEvent.code).isEqualTo(TextClassifierStatsLog.CONVERSATION_ACTIONS_EVENT);
        assertThat(protoEvent.getSessionId()).isEqualTo(sessionId.flattenToString());
        assertThat(protoEvent.getEventType().getNumber())
                .isEqualTo(TextClassifierEvent.TYPE_SELECTION_STARTED);
        assertThat(protoEvent.getModelName()).isEqualTo("model_name");
        assertThat(protoEvent.getWidgetType().getNumber())
                .isEqualTo(TextClassifierEventLogger.WidgetType.WIDGET_TYPE_WEBVIEW);
        assertThat(protoEvent.getFirstEntityType()).isEqualTo("first");
        assertThat(protoEvent.getSecondEntityType()).isEqualTo("second");
        assertThat(protoEvent.getThirdEntityType()).isEqualTo("third");
        assertThat(protoEvent.getScore()).isEqualTo(0.5f);
        assertThat(protoEvent.getPackageName()).isEqualTo(PKG_NAME);
    }

    @Test
    public void writeEvent_languageDetectionEvent() {
        TextClassificationSessionId sessionId = new TextClassificationSessionId();
        TextClassifierEvent.LanguageDetectionEvent languageDetectionEvent =
                new TextClassifierEvent.LanguageDetectionEvent.Builder(
                                TextClassifierEvent.TYPE_SELECTION_STARTED)
                        .setEventContext(createTextClassificationContext())
                        .setModelName("model_name")
                        .setEventIndex(1)
                        .setEntityTypes("en")
                        .setScores(0.5f)
                        .setActionIndices(1)
                        .build();

        mTextClassifierEventLogger.writeEvent(sessionId, languageDetectionEvent);
        ShadowTextClassifierStatsLog.LoggedEvent loggedEvent =
                ShadowTextClassifierStatsLog.getLoggedEvents().get(0);
        AtomsProto.LanguageDetectionEvent protoEvent =
                (AtomsProto.LanguageDetectionEvent) loggedEvent.event;

        assertThat(loggedEvent.code).isEqualTo(TextClassifierStatsLog.LANGUAGE_DETECTION_EVENT);
        assertThat(protoEvent.getSessionId()).isEqualTo(sessionId.flattenToString());
        assertThat(protoEvent.getEventType().getNumber())
                .isEqualTo(TextClassifierEvent.TYPE_SELECTION_STARTED);
        assertThat(protoEvent.getModelName()).isEqualTo("model_name");
        assertThat(protoEvent.getWidgetType().getNumber())
                .isEqualTo(TextClassifierEventLogger.WidgetType.WIDGET_TYPE_WEBVIEW);
        assertThat(protoEvent.getLanguageTag()).isEqualTo("en");
        assertThat(protoEvent.getScore()).isEqualTo(0.5f);
        assertThat(protoEvent.getActionIndex()).isEqualTo(1);
        assertThat(protoEvent.getPackageName()).isEqualTo(PKG_NAME);
    }

    private static TextClassificationContext createTextClassificationContext() {
        return new TextClassificationContext.Builder(PKG_NAME, WIDGET_TYPE).build();
    }
}
