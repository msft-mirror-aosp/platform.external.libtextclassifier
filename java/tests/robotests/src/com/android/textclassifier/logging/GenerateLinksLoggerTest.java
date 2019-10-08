/*
 * Copyright (C) 2017 The Android Open Source Project
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

import static org.junit.Assert.fail;

import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;
import android.view.textclassifier.TextLinks;

import com.android.os.AtomsProto;
import com.android.textclassifier.shadows.ShadowTextClassifierStatsLog;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.Collections;
import java.util.List;
import java.util.Map;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = ShadowTextClassifierStatsLog.class)
public class GenerateLinksLoggerTest {

    private static final String PACKAGE_NAME = "packageName";
    private static final int LATENCY_MS = 123;

    @After
    public void teardown() {
        ShadowTextClassifierStatsLog.reset();
    }

    @Test
    public void logGenerateLinks() {
        final String phoneText = "+12122537077";
        final String addressText = "1600 Amphitheater Parkway, Mountain View, CA";
        final String testText = "The number is " + phoneText + ", the address is " + addressText;
        final int phoneOffset = testText.indexOf(phoneText);
        final int addressOffset = testText.indexOf(addressText);
        final Map<String, Float> phoneEntityScores =
                Collections.singletonMap(TextClassifier.TYPE_PHONE, 1.0f);
        final Map<String, Float> addressEntityScores =
                Collections.singletonMap(TextClassifier.TYPE_ADDRESS, 1.0f);
        TextLinks links =
                new TextLinks.Builder(testText)
                        .addLink(phoneOffset, phoneOffset + phoneText.length(), phoneEntityScores)
                        .addLink(
                                addressOffset,
                                addressOffset + addressText.length(),
                                addressEntityScores)
                        .build();

        GenerateLinksLogger generateLinksLogger = new GenerateLinksLogger(/* sampleRate= */ 1);
        generateLinksLogger.logGenerateLinks(testText, links, PACKAGE_NAME, LATENCY_MS);

        List<ShadowTextClassifierStatsLog.LoggedEvent> loggedEvents =
                ShadowTextClassifierStatsLog.getLoggedEvents();
        assertThat(loggedEvents).hasSize(3);
        assertHasLog(
                loggedEvents,
                /* entityType= */ "",
                /* numLinks= */ 2,
                /* linkedTextLength= */ phoneText.length() + addressText.length(),
                /* textLength= */ testText.length());
        assertHasLog(
                loggedEvents,
                TextClassifier.TYPE_ADDRESS,
                /* numLinks= */ 1,
                /* linkedTextLength= */ addressText.length(),
                /* textLength= */ testText.length());

        assertHasLog(
                loggedEvents,
                TextClassifier.TYPE_PHONE,
                /* numLinks= */ 1,
                /* linkedTextLength= */ phoneText.length(),
                /* textLength= */ testText.length());
    }

    private static void assertHasLog(
            List<ShadowTextClassifierStatsLog.LoggedEvent> loggedEvents,
            String entityType,
            int numLinks,
            int linkedTextLength,
            int textLength) {
        for (ShadowTextClassifierStatsLog.LoggedEvent loggedEvent : loggedEvents) {
            AtomsProto.TextLinkifyEvent protoEvent =
                    (AtomsProto.TextLinkifyEvent) loggedEvent.event;
            if (!entityType.equals(protoEvent.getEntityType())) {
                continue;
            }
            assertThat(protoEvent.getSessionId()).isNotNull();
            assertThat(protoEvent.getEventType().getNumber())
                    .isEqualTo(TextClassifierEvent.TYPE_LINKS_GENERATED);
            assertThat(protoEvent.getPackageName()).isEqualTo(PACKAGE_NAME);
            assertThat(protoEvent.getEntityType()).isEqualTo(entityType);
            assertThat(protoEvent.getNumLinks()).isEqualTo(numLinks);
            assertThat(protoEvent.getLinkedTextLength()).isEqualTo(linkedTextLength);
            assertThat(protoEvent.getTextLength()).isEqualTo(textLength);
            assertThat(protoEvent.getLatencyMillis()).isEqualTo(LATENCY_MS);
            return;
        }
        fail("No log for entity type \"" + entityType + "\"");
    }
}
