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
package com.android.textclassifier;

import android.provider.DeviceConfig;
import android.util.ArrayMap;

import androidx.annotation.GuardedBy;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Retrieves settings from {@link DeviceConfig} and {@link android.provider.Settings}. It will try
 * DeviceConfig first and then Settings.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
public final class ConfigParser {
    private static final String TAG = "ConfigParser";

    private static final String STRING_LIST_DELIMITER = ":";

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private final Map<String, Object> mCache = new ArrayMap<>();

    /** Reads a boolean setting through the cache. */
    public boolean getBoolean(String key, boolean defaultValue) {
        synchronized (mLock) {
            final Object cached = mCache.get(key);
            if (cached instanceof Boolean) {
                return (boolean) cached;
            }
            final boolean value =
                    DeviceConfig.getBoolean(
                            DeviceConfig.NAMESPACE_TEXTCLASSIFIER, key, defaultValue);
            mCache.put(key, value);
            return value;
        }
    }

    /** Reads an integer setting through the cache. */
    public int getInt(String key, int defaultValue) {
        synchronized (mLock) {
            final Object cached = mCache.get(key);
            if (cached instanceof Integer) {
                return (int) cached;
            }
            final int value =
                    DeviceConfig.getInt(DeviceConfig.NAMESPACE_TEXTCLASSIFIER, key, defaultValue);
            mCache.put(key, value);
            return value;
        }
    }

    /** Reads a float setting through the cache. */
    public float getFloat(String key, float defaultValue) {
        synchronized (mLock) {
            final Object cached = mCache.get(key);
            if (cached instanceof Float) {
                return (float) cached;
            }
            final float value =
                    DeviceConfig.getFloat(DeviceConfig.NAMESPACE_TEXTCLASSIFIER, key, defaultValue);
            mCache.put(key, value);
            return value;
        }
    }

    /** Reads a string setting through the cache. */
    public String getString(String key, String defaultValue) {
        synchronized (mLock) {
            final Object cached = mCache.get(key);
            if (cached instanceof String) {
                return (String) cached;
            }
            final String value =
                    DeviceConfig.getString(
                            DeviceConfig.NAMESPACE_TEXTCLASSIFIER, key, defaultValue);
            mCache.put(key, value);
            return value;
        }
    }

    /** Reads a string list setting through the cache. */
    public List<String> getStringList(String key, List<String> defaultValue) {
        synchronized (mLock) {
            final Object cached = mCache.get(key);
            if (cached instanceof List) {
                final List asList = (List) cached;
                if (asList.isEmpty()) {
                    return Collections.emptyList();
                } else if (asList.get(0) instanceof String) {
                    return (List<String>) cached;
                }
            }
            final List<String> value = getDeviceConfigStringList(key, defaultValue);
            mCache.put(key, value);
            return value;
        }
    }

    /**
     * Reads a float array through the cache. The returned array should be expected to be of the
     * same length as that of the defaultValue.
     */
    public float[] getFloatArray(String key, float[] defaultValue) {
        synchronized (mLock) {
            final Object cached = mCache.get(key);
            if (cached instanceof float[]) {
                return (float[]) cached;
            }
            final float[] value = getDeviceConfigFloatArray(key, defaultValue);
            mCache.put(key, value);
            return value;
        }
    }

    private static List<String> getDeviceConfigStringList(String key, List<String> defaultValue) {
        return parse(
                DeviceConfig.getString(DeviceConfig.NAMESPACE_TEXTCLASSIFIER, key, null),
                defaultValue);
    }

    private static float[] getDeviceConfigFloatArray(String key, float[] defaultValue) {
        return parse(
                DeviceConfig.getString(DeviceConfig.NAMESPACE_TEXTCLASSIFIER, key, null),
                defaultValue);
    }

    private static List<String> parse(@Nullable String listStr, List<String> defaultValue) {
        if (listStr != null) {
            return Collections.unmodifiableList(
                    Arrays.asList(listStr.split(STRING_LIST_DELIMITER)));
        }
        return defaultValue;
    }

    private static float[] parse(@Nullable String arrayStr, float[] defaultValue) {
        if (arrayStr != null) {
            final String[] split = arrayStr.split(STRING_LIST_DELIMITER);
            if (split.length != defaultValue.length) {
                return defaultValue;
            }
            final float[] result = new float[split.length];
            for (int i = 0; i < split.length; i++) {
                try {
                    result[i] = Float.parseFloat(split[i]);
                } catch (NumberFormatException e) {
                    return defaultValue;
                }
            }
            return result;
        } else {
            return defaultValue;
        }
    }
}
