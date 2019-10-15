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

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;
import java.util.List;

/**
 * Declares SQLite queries as Java methods with special annotations system.
 *
 * <p>Data Access Object interface crucial for Room database performance. Based on
 * LanguageSignalInfo database entity structure.
 */
@Dao
public interface LanguageSignalInfoDao {

  /**
   * Inserts {@link LanguageSignalInfo} object into the Room database. If there was already entity
   * with the same language local and source type replaces it with a passed one.
   *
   * @param languageSignalInfo object to insert into the database.
   */
  @Insert(onConflict = OnConflictStrategy.REPLACE)
  void insertLanguageInfo(LanguageSignalInfo languageSignalInfo);

  /** Returns all the {@link LanguageSignalInfo} objects which have source like {@code src}. */
  @Query("SELECT * FROM language_signal_infos WHERE source = :src")
  List<LanguageSignalInfo> getBySource(@LanguageSignalInfo.Source int src);

  /** Returns all the {@link LanguageSignalInfo} objects stored in the database. */
  @Query("SELECT * FROM language_signal_infos")
  List<LanguageSignalInfo> getAll();

  /**
   * Increases the count of the specified signal by the specified increment or inserts a new entry
   * if the signal is not in the database yet.
   */
  @Query(
      "INSERT INTO language_signal_infos VALUES(:languageTag, :source, :increment)"
          + "  ON CONFLICT(languageTag, source) DO UPDATE SET count = count + :increment")
  void increaseSignalCount(
      String languageTag, @LanguageSignalInfo.Source int source, int increment);
}
