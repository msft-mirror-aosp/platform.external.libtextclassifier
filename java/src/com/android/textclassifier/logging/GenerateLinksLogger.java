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

import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextClassifierEvent;
import android.view.textclassifier.TextLinks;

import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;
import androidx.core.util.Preconditions;

import com.android.textclassifier.TcLog;
import com.android.textclassifier.TextClassifierStatsLog;

import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.UUID;

/** A helper for logging calls to generateLinks. */
public final class GenerateLinksLogger {

    private static final String LOG_TAG = "GenerateLinksLogger";

    private final Random mRng;
    private final int mSampleRate;

    /**
     * @param sampleRate the rate at which log events are written. (e.g. 100 means there is a 0.01
     *     chance that a call to logGenerateLinks results in an event being written). To write all
     *     events, pass 1.
     */
    public GenerateLinksLogger(int sampleRate) {
        mSampleRate = sampleRate;
        mRng = new Random();
    }

    /** Logs statistics about a call to generateLinks. */
    public void logGenerateLinks(
            CharSequence text, TextLinks links, String callingPackageName, long latencyMs) {
        Preconditions.checkNotNull(text);
        Preconditions.checkNotNull(links);
        Preconditions.checkNotNull(callingPackageName);
        if (!shouldLog()) {
            return;
        }

        // Always populate the total stats, and per-entity stats for each entity type detected.
        final LinkifyStats totalStats = new LinkifyStats();
        final Map<String, LinkifyStats> perEntityTypeStats = new ArrayMap<>();
        for (TextLinks.TextLink link : links.getLinks()) {
            if (link.getEntityCount() == 0) continue;
            final String entityType = link.getEntity(0);
            if (entityType == null
                    || TextClassifier.TYPE_OTHER.equals(entityType)
                    || TextClassifier.TYPE_UNKNOWN.equals(entityType)) {
                continue;
            }
            totalStats.countLink(link);
            perEntityTypeStats.computeIfAbsent(entityType, k -> new LinkifyStats()).countLink(link);
        }

        final String callId = UUID.randomUUID().toString();
        writeStats(callId, callingPackageName, null, totalStats, text, latencyMs);
        for (Map.Entry<String, LinkifyStats> entry : perEntityTypeStats.entrySet()) {
            writeStats(
                    callId, callingPackageName, entry.getKey(), entry.getValue(), text, latencyMs);
        }
    }

    /**
     * Returns whether this particular event should be logged.
     *
     * <p>Sampling is used to reduce the amount of logging data generated.
     */
    private boolean shouldLog() {
        if (mSampleRate <= 1) {
            return true;
        } else {
            return mRng.nextInt(mSampleRate) == 0;
        }
    }

    /** Writes a log event for the given stats. */
    private void writeStats(
            String callId,
            String callingPackageName,
            @Nullable String entityType,
            LinkifyStats stats,
            CharSequence text,
            long latencyMs) {
        TextClassifierStatsLog.write(
                TextClassifierStatsLog.TEXT_LINKIFY_EVENT,
                callId,
                TextClassifierEvent.TYPE_LINKS_GENERATED,
                /*modelName=*/ null,
                TextClassifierEventLogger.WidgetType.WIDGET_TYPE_UNKNOWN,
                /*eventIndex=*/ 0,
                entityType,
                stats.mNumLinks,
                stats.mNumLinksTextLength,
                text.length(),
                latencyMs,
                callingPackageName);
        if (TcLog.ENABLE_FULL_LOGGING) {
            TcLog.v(
                    LOG_TAG,
                    String.format(
                            Locale.US,
                            "%s:%s %d links (%d/%d chars) %dms %s",
                            callId,
                            entityType,
                            stats.mNumLinks,
                            stats.mNumLinksTextLength,
                            text.length(),
                            latencyMs,
                            callingPackageName));
        }
    }

    /** Helper class for storing per-entity type statistics. */
    private static final class LinkifyStats {
        int mNumLinks;
        int mNumLinksTextLength;

        void countLink(TextLinks.TextLink link) {
            mNumLinks += 1;
            mNumLinksTextLength += link.getEnd() - link.getStart();
        }
    }
}
