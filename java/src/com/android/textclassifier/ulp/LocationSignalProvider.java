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

package com.android.textclassifier.ulp;

import android.content.Context;
import android.icu.util.ULocale;
import android.location.Address;
import android.location.Geocoder;
import android.location.Location;
import android.location.LocationManager;
import android.telephony.TelephonyManager;

import androidx.annotation.Nullable;

import com.android.textclassifier.TcLog;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.TimeUnit;

final class LocationSignalProvider {
    private static final String TAG = "LocationSignalProvider";

    private static final long EXPIRATION_TIME = TimeUnit.DAYS.toMillis(1);
    private final LocationManager mLocationManager;
    private final TelephonyManager mTelephonyManager;
    private final Geocoder mGeocoder;
    private final Object mLock = new Object();

    private String mCachedLanguageTag = null;
    private long mLastModifiedTime;

    LocationSignalProvider(Context context) {
        this(
                context.getSystemService(LocationManager.class),
                context.getSystemService(TelephonyManager.class),
                new Geocoder(context));
    }

    @VisibleForTesting
    LocationSignalProvider(
            LocationManager locationManager, TelephonyManager telephonyManager, Geocoder geocoder) {
        mLocationManager = Preconditions.checkNotNull(locationManager);
        mTelephonyManager = Preconditions.checkNotNull(telephonyManager);
        mGeocoder = Preconditions.checkNotNull(geocoder);
    }

    /**
     * Deduces the language by using user's current location as a signal and returns a BCP 47
     * language code.
     */
    @Nullable
    String detectLanguageTag() {
        synchronized (mLock) {
            if ((System.currentTimeMillis() - mLastModifiedTime) < EXPIRATION_TIME) {
                return mCachedLanguageTag;
            }
            mCachedLanguageTag = detectLanguageCodeInternal();
            mLastModifiedTime = System.currentTimeMillis();
            return mCachedLanguageTag;
        }
    }

    @Nullable
    private String detectLanguageCodeInternal() {
        String currentCountryCode = detectCurrentCountryCode();
        if (currentCountryCode == null) {
            return null;
        }
        return toLanguageTag(currentCountryCode);
    }

    @Nullable
    private String detectCurrentCountryCode() {
        String networkCountryCode = mTelephonyManager.getNetworkCountryIso();
        if (networkCountryCode != null) {
            return networkCountryCode;
        }
        return detectCurrentCountryFromLocationManager();
    }

    @Nullable
    private String detectCurrentCountryFromLocationManager() {
        Location location = mLocationManager.getLastKnownLocation(LocationManager.PASSIVE_PROVIDER);
        if (location == null) {
            return null;
        }
        try {
            List<Address> addresses =
                    mGeocoder.getFromLocation(
                            location.getLatitude(), location.getLongitude(), /*maxResults=*/ 1);
            if (addresses != null && !addresses.isEmpty()) {
                return addresses.get(0).getCountryCode();
            }
        } catch (IOException e) {
            TcLog.e(TAG, "Failed to call getFromLocation: ", e);
            return null;
        }
        return null;
    }

    @Nullable
    private static String toLanguageTag(String countryTag) {
        ULocale locale = new ULocale.Builder().setRegion(countryTag).build();
        locale = ULocale.addLikelySubtags(locale);
        return locale.getLanguage();
    }
}
