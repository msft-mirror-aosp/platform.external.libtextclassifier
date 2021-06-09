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

package com.android.textclassifier.common.statsd;

import android.util.Log;
import androidx.test.core.app.ApplicationProvider;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.TextClassifierDownloadReported;
import com.google.common.collect.ImmutableList;
import java.util.stream.Collectors;
import org.junit.rules.ExternalResource;

// TODO(licha): Make this generic and useful for other atoms.
/** Test rule to set up/clean up statsd for download logger tests. */
public final class TextClassifierDownloadLoggerTestRule extends ExternalResource {
  private static final String TAG = "DownloadLoggerTestRule";

  /** A statsd config ID, which is arbitrary. */
  private static final long CONFIG_ID = 423779;

  private static final long SHORT_TIMEOUT_MS = 1000;

  @Override
  public void before() throws Exception {
    StatsdTestUtils.cleanup(CONFIG_ID);

    StatsdConfig.Builder builder =
        StatsdConfig.newBuilder()
            .setId(CONFIG_ID)
            .addAllowedLogSource(ApplicationProvider.getApplicationContext().getPackageName());
    StatsdTestUtils.addAtomMatcher(builder, Atom.TEXT_CLASSIFIER_DOWNLOAD_REPORTED_FIELD_NUMBER);
    StatsdTestUtils.pushConfig(builder.build());
  }

  @Override
  public void after() {
    try {
      StatsdTestUtils.cleanup(CONFIG_ID);
    } catch (Exception e) {
      Log.e(TAG, "Failed to clean up statsd after tests.");
    }
  }

  /** Gets a list of download atoms written into statsd, sorted by increasing timestamp. */
  public ImmutableList<TextClassifierDownloadReported> getLoggedAtoms() throws Exception {
    ImmutableList<Atom> loggedAtoms = StatsdTestUtils.getLoggedAtoms(CONFIG_ID, SHORT_TIMEOUT_MS);
    return ImmutableList.copyOf(
        loggedAtoms.stream()
            .map(Atom::getTextClassifierDownloadReported)
            .collect(Collectors.toList()));
  }
}
