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

package com.android.textclassifier.testing;

import android.app.UiAutomation;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.provider.DeviceConfig;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;
import org.junit.rules.ExternalResource;

/** A rule that manages a text classifier that is backed by the ExtServices. */
public final class ExtServicesTextClassifierRule extends ExternalResource {
  private static final String CONFIG_TEXT_CLASSIFIER_SERVICE_PACKAGE_OVERRIDE =
      "textclassifier_service_package_override";
  private static final String PKG_NAME_GOOGLE_EXTSERVICES = "com.google.android.ext.services";
  private static final String PKG_NAME_AOSP_EXTSERVICES = "android.ext.services";

  private String textClassifierServiceOverrideFlagOldValue;

  @Override
  protected void before() {
    UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
    try {
      uiAutomation.adoptShellPermissionIdentity();
      textClassifierServiceOverrideFlagOldValue =
          DeviceConfig.getString(
              DeviceConfig.NAMESPACE_TEXTCLASSIFIER,
              CONFIG_TEXT_CLASSIFIER_SERVICE_PACKAGE_OVERRIDE,
              null);
      DeviceConfig.setProperty(
          DeviceConfig.NAMESPACE_TEXTCLASSIFIER,
          CONFIG_TEXT_CLASSIFIER_SERVICE_PACKAGE_OVERRIDE,
          getExtServicesPackageName(),
          /* makeDefault= */ false);
    } finally {
      uiAutomation.dropShellPermissionIdentity();
    }
  }

  @Override
  protected void after() {
    UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
    try {
      uiAutomation.adoptShellPermissionIdentity();
      DeviceConfig.setProperty(
          DeviceConfig.NAMESPACE_TEXTCLASSIFIER,
          CONFIG_TEXT_CLASSIFIER_SERVICE_PACKAGE_OVERRIDE,
          textClassifierServiceOverrideFlagOldValue,
          /* makeDefault= */ false);
    } finally {
      uiAutomation.dropShellPermissionIdentity();
    }
  }

  private static String getExtServicesPackageName() {
    PackageManager packageManager = ApplicationProvider.getApplicationContext().getPackageManager();
    try {
      packageManager.getApplicationInfo(PKG_NAME_GOOGLE_EXTSERVICES, /* flags= */ 0);
      return PKG_NAME_GOOGLE_EXTSERVICES;
    } catch (NameNotFoundException e) {
      return PKG_NAME_AOSP_EXTSERVICES;
    }
  }

  public TextClassifier getTextClassifier() {
    TextClassificationManager textClassificationManager =
        ApplicationProvider.getApplicationContext()
            .getSystemService(TextClassificationManager.class);
    textClassificationManager.setTextClassifier(null); // Reset TC overrides
    return textClassificationManager.getTextClassifier();
  }
}
