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

package com.android.textclassifier.ulp;

import android.content.Context;
import android.icu.util.ULocale;
import android.location.Address;
import android.location.Geocoder;
import android.location.Location;
import android.location.LocationManager;
import android.telephony.TelephonyManager;
import com.android.textclassifier.TcLog;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;
import java.io.IOException;
import java.util.List;
import java.util.concurrent.TimeUnit;
import javax.annotation.Nullable;

final class LocationSignalProvider {
  private static final String TAG = "LocationSignalProvider";

  private static final long EXPIRATION_TIME = TimeUnit.DAYS.toMillis(1);
  private final LocationManager locationManager;
  private final TelephonyManager telephonyManager;
  private final Geocoder geocoder;
  private final Object lock = new Object();

  private String cachedLanguageTag = null;
  private long lastModifiedTime;

  LocationSignalProvider(Context context) {
    this(
        context.getSystemService(LocationManager.class),
        context.getSystemService(TelephonyManager.class),
        new Geocoder(context));
  }

  @VisibleForTesting
  LocationSignalProvider(
      LocationManager locationManager, TelephonyManager telephonyManager, Geocoder geocoder) {
    this.locationManager = Preconditions.checkNotNull(locationManager);
    this.telephonyManager = Preconditions.checkNotNull(telephonyManager);
    this.geocoder = Preconditions.checkNotNull(geocoder);
  }

  /**
   * Deduces the language by using user's current location as a signal and returns a BCP 47 language
   * code.
   */
  @Nullable
  String detectLanguageTag() {
    synchronized (lock) {
      if ((System.currentTimeMillis() - lastModifiedTime) < EXPIRATION_TIME) {
        return cachedLanguageTag;
      }
      cachedLanguageTag = detectLanguageCodeInternal();
      lastModifiedTime = System.currentTimeMillis();
      return cachedLanguageTag;
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
    String networkCountryCode = telephonyManager.getNetworkCountryIso();
    if (networkCountryCode != null) {
      return networkCountryCode;
    }
    return detectCurrentCountryFromLocationManager();
  }

  @Nullable
  private String detectCurrentCountryFromLocationManager() {
    Location location = locationManager.getLastKnownLocation(LocationManager.PASSIVE_PROVIDER);
    if (location == null) {
      return null;
    }
    try {
      List<Address> addresses =
          geocoder.getFromLocation(
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
