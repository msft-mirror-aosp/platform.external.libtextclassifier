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

package com.android.textclassifier;

import android.content.Context;
import android.database.ContentObserver;
import android.provider.Settings;
import androidx.annotation.GuardedBy;
import com.android.textclassifier.common.base.TcLog;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import java.lang.ref.WeakReference;
import java.util.Objects;
import java.util.function.Supplier;
import javax.annotation.Nullable;

/** Parses the {@link Settings.Global#TEXT_CLASSIFIER_ACTION_MODEL_PARAMS} flag. */
// TODO(tonymak): Re-enable this class.
public final class ActionsModelParamsSupplier
    implements Supplier<ActionsModelParamsSupplier.ActionsModelParams> {
  private static final String TAG = "ActionsModelParams";

  @VisibleForTesting static final String KEY_REQUIRED_MODEL_VERSION = "required_model_version";
  @VisibleForTesting static final String KEY_REQUIRED_LOCALES = "required_locales";

  @VisibleForTesting static final String KEY_SERIALIZED_PRECONDITIONS = "serialized_preconditions";

  private final Context appContext;
  private final SettingsObserver settingsObserver;

  private final Object lock = new Object();
  private final Runnable onChangedListener;

  @Nullable
  @GuardedBy("lock")
  private ActionsModelParams actionsModelParams;

  @GuardedBy("lock")
  private boolean parsed = true;

  public ActionsModelParamsSupplier(Context context, @Nullable Runnable onChangedListener) {
    appContext = Preconditions.checkNotNull(context).getApplicationContext();
    this.onChangedListener = onChangedListener == null ? () -> {} : onChangedListener;
    settingsObserver =
        new SettingsObserver(
            appContext,
            () -> {
              synchronized (lock) {
                TcLog.v(TAG, "Settings.Global.TEXT_CLASSIFIER_ACTION_MODEL_PARAMS is updated");
                parsed = true;
                this.onChangedListener.run();
              }
            });
  }

  /**
   * Returns the parsed actions params or {@link ActionsModelParams#INVALID} if the value is
   * invalid.
   */
  @Override
  public ActionsModelParams get() {
    synchronized (lock) {
      if (parsed) {
        actionsModelParams = parse();
        parsed = false;
      }
      return actionsModelParams;
    }
  }

  private static ActionsModelParams parse() {
    //        String settingStr = Settings.Global.getString(contentResolver,
    //                Settings.Global.TEXT_CLASSIFIER_ACTION_MODEL_PARAMS);
    //        if (TextUtils.isEmpty(settingStr)) {
    //            return ActionsModelParams.INVALID;
    //        }
    //        try {
    //            KeyValueListParser keyValueListParser = new KeyValueListParser(',');
    //            keyValueListParser.setString(settingStr);
    //            int version = keyValueListParser.getInt(KEY_REQUIRED_MODEL_VERSION, -1);
    //            if (version == -1) {
    //                TcLog.w(TAG, "ActionsModelParams.Parse, invalid model version");
    //                return ActionsModelParams.INVALID;
    //            }
    //            String locales = keyValueListParser.getString(KEY_REQUIRED_LOCALES, null);
    //            if (locales == null) {
    //                TcLog.w(TAG, "ActionsModelParams.Parse, invalid locales");
    //                return ActionsModelParams.INVALID;
    //            }
    //            String serializedPreconditionsStr =
    //                    keyValueListParser.getString(KEY_SERIALIZED_PRECONDITIONS, null);
    //            if (serializedPreconditionsStr == null) {
    //                TcLog.w(TAG, "ActionsModelParams.Parse, invalid preconditions");
    //                return ActionsModelParams.INVALID;
    //            }
    //            byte[] serializedPreconditions =
    //                    Base64.decode(serializedPreconditionsStr, Base64.NO_WRAP);
    //            return new ActionsModelParams(version, locales, serializedPreconditions);
    //        } catch (Throwable t) {
    //            TcLog.e(TAG, "Invalid TEXT_CLASSIFIER_ACTION_MODEL_PARAMS, ignore", t);
    //        }
    return ActionsModelParams.INVALID;
  }

  @Override
  protected void finalize() throws Throwable {
    try {
      appContext.getContentResolver().unregisterContentObserver(settingsObserver);
    } finally {
      super.finalize();
    }
  }

  /** Represents the parsed result. */
  public static final class ActionsModelParams {

    public static final ActionsModelParams INVALID = new ActionsModelParams(-1, "", new byte[0]);

    /** The required model version to apply {@code serializedPreconditions}. */
    private final int requiredModelVersion;

    /** The required model locales to apply {@code serializedPreconditions}. */
    private final String requiredModelLocales;

    /**
     * The serialized params that will be applied to the model file, if all requirements are met. Do
     * not modify.
     */
    private final byte[] serializedPreconditions;

    public ActionsModelParams(
        int requiredModelVersion, String requiredModelLocales, byte[] serializedPreconditions) {
      this.requiredModelVersion = requiredModelVersion;
      this.requiredModelLocales = Preconditions.checkNotNull(requiredModelLocales);
      this.serializedPreconditions = Preconditions.checkNotNull(serializedPreconditions);
    }

    /**
     * Returns the serialized preconditions. Returns {@code null} if the model in use does not meet
     * all the requirements listed in the {@code ActionsModelParams} or the params are invalid.
     */
    @Nullable
    public byte[] getSerializedPreconditions(ModelFileManager.ModelFile modelInUse) {
      if (this == INVALID) {
        return null;
      }
      if (modelInUse.getVersion() != requiredModelVersion) {
        TcLog.w(
            TAG,
            String.format(
                "Not applying mSerializedPreconditions, required version=%d, actual=%d",
                requiredModelVersion, modelInUse.getVersion()));
        return null;
      }
      if (!Objects.equals(modelInUse.getSupportedLocalesStr(), requiredModelLocales)) {
        TcLog.w(
            TAG,
            String.format(
                "Not applying mSerializedPreconditions, required locales=%s, actual=%s",
                requiredModelLocales, modelInUse.getSupportedLocalesStr()));
        return null;
      }
      return serializedPreconditions;
    }
  }

  private static final class SettingsObserver extends ContentObserver {

    private final WeakReference<Runnable> onChangedListener;

    SettingsObserver(Context appContext, Runnable listener) {
      super(null);
      onChangedListener = new WeakReference<>(listener);
      //            appContext.getContentResolver().registerContentObserver(
      //
      // Settings.Global.getUriFor(Settings.Global.TEXT_CLASSIFIER_ACTION_MODEL_PARAMS),
      //                    false /* notifyForDescendants */,
      //                    this);
    }

    @Override
    public void onChange(boolean selfChange) {
      if (onChangedListener.get() != null) {
        onChangedListener.get().run();
      }
    }
  }
}
