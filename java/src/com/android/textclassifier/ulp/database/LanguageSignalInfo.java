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

package com.android.textclassifier.ulp.database;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.room.ColumnInfo;
import androidx.room.Entity;
import com.google.common.base.Preconditions;
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

  /** The source of the signal */
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({SUGGEST_CONVERSATION_ACTIONS, CLASSIFY_TEXT})
  public @interface Source {}

  public static final int SUGGEST_CONVERSATION_ACTIONS = 0;
  public static final int CLASSIFY_TEXT = 1;

  @NonNull
  @ColumnInfo(name = "languageTag")
  private final String languageTag;

  @ColumnInfo(name = "source")
  private final int source;

  @ColumnInfo(name = "count")
  private final int count;

  public LanguageSignalInfo(String languageTag, @Source int source, int count) {
    this.languageTag = Preconditions.checkNotNull(languageTag);
    this.source = source;
    this.count = count;
  }

  public String getLanguageTag() {
    return languageTag;
  }

  @Source
  public int getSource() {
    return source;
  }

  public int getCount() {
    return count;
  }

  @Override
  public String toString() {
    String src = "OTHER";
    if (source == SUGGEST_CONVERSATION_ACTIONS) {
      src = "SUGGEST_CONVERSATION_ACTIONS";
    } else if (source == CLASSIFY_TEXT) {
      src = "CLASSIFY_TEXT";
    }

    return languageTag + "_" + src + ": " + count;
  }

  @Override
  public int hashCode() {
    int result = languageTag.hashCode();
    result = 31 * result + Integer.hashCode(source);
    result = 31 * result + Integer.hashCode(count);
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
    return languageTag.equals(info.getLanguageTag())
        && source == info.getSource()
        && count == info.getCount();
  }
}
