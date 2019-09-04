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

package com.android.textclassifier.ulp.database;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.core.util.Preconditions;
import androidx.room.ColumnInfo;
import androidx.room.Entity;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class defining Room database entity.
 *
 * <p>Represents information about number of signals received from one of defined source types in
 * specified language.
 */
@Entity(
        tableName = "language_signal_infos",
        primaryKeys = {"languageTag", "source"})
public final class LanguageSignalInfo {

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({SUGGEST_CONVERSATION_ACTIONS, CLASSIFY_TEXT})
    public @interface Source {}

    public static final int SUGGEST_CONVERSATION_ACTIONS = 0;
    public static final int CLASSIFY_TEXT = 1;

    @NonNull
    @ColumnInfo(name = "languageTag")
    private String mLanguageTag;

    @ColumnInfo(name = "source")
    private int mSource;

    @ColumnInfo(name = "count")
    private int mCount;

    public LanguageSignalInfo(String languageTag, @Source int source, int count) {
        mLanguageTag = Preconditions.checkNotNull(languageTag);
        mSource = source;
        mCount = count;
    }

    public String getLanguageTag() {
        return mLanguageTag;
    }

    @Source
    public int getSource() {
        return mSource;
    }

    public int getCount() {
        return mCount;
    }

    @Override
    public String toString() {
        String src = "OTHER";
        if (mSource == SUGGEST_CONVERSATION_ACTIONS) {
            src = "SUGGEST_CONVERSATION_ACTIONS";
        } else if (mSource == CLASSIFY_TEXT) {
            src = "CLASSIFY_TEXT";
        }

        return mLanguageTag + "_" + src + ": " + mCount;
    }

    @Override
    public int hashCode() {
        int result = mLanguageTag.hashCode();
        result = 31 * result + Integer.hashCode(mSource);
        result = 31 * result + Integer.hashCode(mCount);
        return result;
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null || obj.getClass() != getClass()) {
            return false;
        }
        LanguageSignalInfo info = (LanguageSignalInfo) obj;
        return mLanguageTag.equals(info.getLanguageTag())
                && mSource == info.getSource()
                && mCount == info.getCount();
    }
}
