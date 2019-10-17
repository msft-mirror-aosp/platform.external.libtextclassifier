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

import android.content.Context;
import androidx.annotation.GuardedBy;
import androidx.room.Database;
import androidx.room.Room;
import androidx.room.RoomDatabase;

/**
 * Represents user language profile database.
 *
 * <p>Needed for Room database to work. Allows to create a database instance (or get an already
 * existing if there is one) and use it.
 */
@Database(
    entities = {LanguageSignalInfo.class},
    version = 1,
    exportSchema = false)
public abstract class LanguageProfileDatabase extends RoomDatabase {
  private static final Object lock = new Object();

  @GuardedBy("lock")
  private static LanguageProfileDatabase instance;

  /**
   * Returns {@link LanguageSignalInfoDao} object belonging to the {@link LanguageProfileDatabase}
   * with which we can call database queries.
   */
  public abstract LanguageSignalInfoDao languageInfoDao();

  /**
   * Create an instance of {@link LanguageProfileDatabase} for chosen context or existing one if it
   * was already created.
   */
  public static LanguageProfileDatabase getInstance(final Context context) {
    synchronized (lock) {
      if (instance == null) {
        instance =
            Room.databaseBuilder(
                    context.getApplicationContext(),
                    LanguageProfileDatabase.class,
                    "language_profile")
                .build();
      }
      return instance;
    }
  }
}
