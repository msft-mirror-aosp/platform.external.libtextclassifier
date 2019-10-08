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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import android.location.Address;
import android.location.Geocoder;
import android.location.Location;
import android.location.LocationManager;
import android.telephony.TelephonyManager;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.io.IOException;
import java.util.Collections;
import java.util.Locale;

@SmallTest
public class LocationSignalProviderTest {
    @Mock private LocationManager mLocationManager;
    @Mock private TelephonyManager mTelephonyManager;
    @Mock private LocationSignalProvider mLocationSignalProvider;
    @Mock private Geocoder mGeocoder;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mLocationSignalProvider =
                new LocationSignalProvider(mLocationManager, mTelephonyManager, mGeocoder);
    }

    @Test
    public void detectLanguageTag_useTelephony() {
        when(mTelephonyManager.getNetworkCountryIso()).thenReturn(Locale.UK.getCountry());

        assertThat(mLocationSignalProvider.detectLanguageTag()).isEqualTo("en");
    }

    @Test
    public void detectLanguageTag_useLocation() throws IOException {
        when(mTelephonyManager.getNetworkCountryIso()).thenReturn(null);
        Location location = new Location(LocationManager.PASSIVE_PROVIDER);
        when(mLocationManager.getLastKnownLocation(LocationManager.PASSIVE_PROVIDER))
                .thenReturn(location);
        Address address = new Address(Locale.FRANCE);
        address.setCountryCode(Locale.FRANCE.getCountry());
        when(mGeocoder.getFromLocation(Mockito.anyDouble(), Mockito.anyDouble(), Mockito.anyInt()))
                .thenReturn(Collections.singletonList(address));

        assertThat(mLocationSignalProvider.detectLanguageTag()).isEqualTo("fr");
    }
}
