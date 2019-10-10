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

package com.android.textclassifier.shadows;

import android.stats.textclassifier.EventType;
import android.stats.textclassifier.WidgetType;

import com.android.os.AtomsProto;
import com.android.textclassifier.TextClassifierStatsLog;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import java.util.ArrayList;
import java.util.List;

/**
 * Shadows of {@link TextClassifierStatsLog}.
 *
 * <p>It is difficult to verify code that calls {@link TextClassifierStatsLog#write} methods as they
 * are static methods in a final class. This shadow is thus introduced, it intercepts the various
 * write() methods and convert them into proto objects which can be verified easily.
 */
@Implements(TextClassifierStatsLog.class)
public class ShadowTextClassifierStatsLog {

    private static List<LoggedEvent> sLoggedEvents = new ArrayList<>();

    @Implementation
    public static void write(
            int code,
            java.lang.String arg1,
            int arg2,
            java.lang.String arg3,
            int arg4,
            int arg5,
            java.lang.String arg6,
            int arg7,
            int arg8,
            int arg9,
            int arg10,
            String arg11) {
        sLoggedEvents.add(
                new LoggedEvent(
                        code,
                        AtomsProto.TextSelectionEvent.newBuilder()
                                .setSessionId(arg1)
                                .setEventType(EventType.forNumber(arg2))
                                .setModelName(emptyStringIfNull(arg3))
                                .setWidgetType(WidgetType.forNumber(arg4))
                                .setEventIndex(arg5)
                                .setEntityType(arg6)
                                .setRelativeWordStartIndex(arg7)
                                .setRelativeWordEndIndex(arg8)
                                .setRelativeSuggestedWordStartIndex(arg9)
                                .setRelativeSuggestedWordEndIndex(arg10)
                                .setPackageName(arg11)
                                .build()));
    }

    @Implementation
    public static void write(
            int code,
            java.lang.String arg1,
            int arg2,
            java.lang.String arg3,
            int arg4,
            int arg5,
            java.lang.String arg6,
            int arg7,
            int arg8,
            int arg9,
            long arg10,
            String arg11) {
        sLoggedEvents.add(
                new LoggedEvent(
                        code,
                        AtomsProto.TextLinkifyEvent.newBuilder()
                                .setSessionId(arg1)
                                .setEventType(EventType.forNumber(arg2))
                                .setModelName(emptyStringIfNull(arg3))
                                .setWidgetType(WidgetType.forNumber(arg4))
                                .setEventIndex(arg5)
                                .setEntityType(emptyStringIfNull(arg6))
                                .setNumLinks(arg7)
                                .setLinkedTextLength(arg8)
                                .setTextLength(arg9)
                                .setLatencyMillis(arg10)
                                .setPackageName(arg11)
                                .build()));
    }

    @Implementation
    public static void write(
            int code,
            java.lang.String arg1,
            int arg2,
            java.lang.String arg3,
            int arg4,
            java.lang.String arg5,
            java.lang.String arg6,
            java.lang.String arg7,
            float arg8,
            String arg9) {
        sLoggedEvents.add(
                new LoggedEvent(
                        code,
                        AtomsProto.ConversationActionsEvent.newBuilder()
                                .setSessionId(arg1)
                                .setEventType(EventType.forNumber(arg2))
                                .setModelName(arg3)
                                .setWidgetType(WidgetType.forNumber(arg4))
                                .setFirstEntityType(arg5)
                                .setSecondEntityType(arg6)
                                .setThirdEntityType(arg7)
                                .setScore(arg8)
                                .setPackageName(arg9)
                                .build()));
    }

    @Implementation
    public static void write(
            int code,
            java.lang.String arg1,
            int arg2,
            java.lang.String arg3,
            int arg4,
            java.lang.String arg5,
            float arg6,
            int arg7,
            String arg8) {
        sLoggedEvents.add(
                new LoggedEvent(
                        code,
                        AtomsProto.LanguageDetectionEvent.newBuilder()
                                .setSessionId(arg1)
                                .setEventType(EventType.forNumber(arg2))
                                .setModelName(arg3)
                                .setWidgetType(WidgetType.forNumber(arg4))
                                .setLanguageTag(arg5)
                                .setScore(arg6)
                                .setActionIndex(arg7)
                                .setPackageName(arg8)
                                .build()));
    }

    public static List<LoggedEvent> getLoggedEvents() {
        return new ArrayList<>(sLoggedEvents);
    }

    private static String emptyStringIfNull(String str) {
        return str == null ? "" : str;
    }

    @Resetter
    public static void reset() {
        sLoggedEvents.clear();
    }

    public static class LoggedEvent {
        public final int code;
        public final Object event;

        LoggedEvent(int code, Object event) {
            this.code = code;
            this.event = event;
        }
    }
}
