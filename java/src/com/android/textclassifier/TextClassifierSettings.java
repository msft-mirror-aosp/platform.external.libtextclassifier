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

import android.provider.DeviceConfig;
import android.view.textclassifier.ConversationAction;
import android.view.textclassifier.TextClassifier;
import androidx.annotation.NonNull;
import com.android.textclassifier.ModelFileManager.ModelFile.ModelType;
import com.android.textclassifier.utils.IndentingPrintWriter;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import javax.annotation.Nullable;

/**
 * TextClassifier specific settings.
 *
 * <p>Currently, this class does not guarantee co-diverted flags are updated atomically.
 *
 * <p>Example of setting the values for testing.
 *
 * <pre>
 * adb shell cmd device_config put textclassifier system_textclassifier_enabled true
 * </pre>
 *
 * @see android.provider.DeviceConfig#NAMESPACE_TEXTCLASSIFIER
 */
public final class TextClassifierSettings {
  public static final String NAMESPACE = DeviceConfig.NAMESPACE_TEXTCLASSIFIER;

  private static final String DELIMITER = ":";

  /** Whether the user language profile feature is enabled. */
  private static final String USER_LANGUAGE_PROFILE_ENABLED = "user_language_profile_enabled";
  /** Max length of text that suggestSelection can accept. */
  @VisibleForTesting
  static final String SUGGEST_SELECTION_MAX_RANGE_LENGTH = "suggest_selection_max_range_length";
  /** Max length of text that classifyText can accept. */
  private static final String CLASSIFY_TEXT_MAX_RANGE_LENGTH = "classify_text_max_range_length";
  /** Max length of text that generateLinks can accept. */
  private static final String GENERATE_LINKS_MAX_TEXT_LENGTH = "generate_links_max_text_length";
  /** Sampling rate for generateLinks logging. */
  private static final String GENERATE_LINKS_LOG_SAMPLE_RATE = "generate_links_log_sample_rate";
  /**
   * Extra count that is added to some languages, e.g. system languages, when deducing the frequent
   * languages in {@link
   * com.android.textclassifier.ulp.LanguageProfileAnalyzer#getFrequentLanguages(int)}.
   */

  /**
   * A colon(:) separated string that specifies the default entities types for generateLinks when
   * hint is not given.
   */
  @VisibleForTesting static final String ENTITY_LIST_DEFAULT = "entity_list_default";
  /**
   * A colon(:) separated string that specifies the default entities types for generateLinks when
   * the text is in a not editable UI widget.
   */
  private static final String ENTITY_LIST_NOT_EDITABLE = "entity_list_not_editable";
  /**
   * A colon(:) separated string that specifies the default entities types for generateLinks when
   * the text is in an editable UI widget.
   */
  private static final String ENTITY_LIST_EDITABLE = "entity_list_editable";
  /**
   * A colon(:) separated string that specifies the default action types for
   * suggestConversationActions when the suggestions are used in an app.
   */
  private static final String IN_APP_CONVERSATION_ACTION_TYPES_DEFAULT =
      "in_app_conversation_action_types_default";
  /**
   * A colon(:) separated string that specifies the default action types for
   * suggestConversationActions when the suggestions are used in a notification.
   */
  private static final String NOTIFICATION_CONVERSATION_ACTION_TYPES_DEFAULT =
      "notification_conversation_action_types_default";
  /** Threshold to accept a suggested language from LangID model. */
  @VisibleForTesting static final String LANG_ID_THRESHOLD_OVERRIDE = "lang_id_threshold_override";
  /** Whether to enable {@link com.android.textclassifier.intent.TemplateIntentFactory}. */
  @VisibleForTesting
  static final String TEMPLATE_INTENT_FACTORY_ENABLED = "template_intent_factory_enabled";
  /** Whether to enable "translate" action in classifyText. */
  private static final String TRANSLATE_IN_CLASSIFICATION_ENABLED =
      "translate_in_classification_enabled";
  /**
   * Whether to detect the languages of the text in request by using langId for the native model.
   */
  private static final String DETECT_LANGUAGES_FROM_TEXT_ENABLED =
      "detect_languages_from_text_enabled";

  /** Whether to enable model downloading with ModelDownloadManager */
  @VisibleForTesting
  static final String MODEL_DOWNLOAD_MANAGER_ENABLED = "model_download_manager_enabled";
  /** Max attempts allowed for a single ModelDownloader downloading task. */
  @VisibleForTesting
  static final String MODEL_DOWNLOAD_MAX_ATTEMPTS = "model_download_max_attempts";
  /** The prefix of the URL to download models. E.g. https://www.gstatic.com/android/ */
  @VisibleForTesting static final String ANNOTATOR_URL_PREFIX = "annotator_url_prefix";

  @VisibleForTesting static final String LANG_ID_URL_PREFIX = "lang_id_url_prefix";

  @VisibleForTesting
  static final String ACTIONS_SUGGESTIONS_URL_PREFIX = "actions_suggestions_url_prefix";
  /** The suffix of the URL to download models. E.g. q/711/en.fb */
  @VisibleForTesting
  static final String PRIMARY_ANNOTATOR_URL_SUFFIX = "primary_annotator_url_suffix";

  @VisibleForTesting static final String PRIMARY_LANG_ID_URL_SUFFIX = "primary_lang_id_url_suffix";

  @VisibleForTesting
  static final String PRIMARY_ACTIONS_SUGGESTIONS_URL_SUFFIX =
      "primary_actions_suggestions_url_suffix";

  /**
   * A colon(:) separated string that specifies the configuration to use when including surrounding
   * context text in language detection queries.
   *
   * <p>Format= minimumTextSize<int>:penalizeRatio<float>:textScoreRatio<float>
   *
   * <p>e.g. 20:1.0:0.4
   *
   * <p>Accept all text lengths with minimumTextSize=0
   *
   * <p>Reject all text less than minimumTextSize with penalizeRatio=0
   *
   * @see {@code TextClassifierImpl#detectLanguages(String, int, int)} for reference.
   */
  @VisibleForTesting static final String LANG_ID_CONTEXT_SETTINGS = "lang_id_context_settings";
  /** Default threshold to translate the language of the context the user selects */
  private static final String TRANSLATE_ACTION_THRESHOLD = "translate_action_threshold";

  // Sync this with ConversationAction.TYPE_ADD_CONTACT;
  public static final String TYPE_ADD_CONTACT = "add_contact";
  // Sync this with ConversationAction.COPY;
  public static final String TYPE_COPY = "copy";

  private static final int SUGGEST_SELECTION_MAX_RANGE_LENGTH_DEFAULT = 10 * 1000;
  private static final int CLASSIFY_TEXT_MAX_RANGE_LENGTH_DEFAULT = 10 * 1000;
  private static final int GENERATE_LINKS_MAX_TEXT_LENGTH_DEFAULT = 100 * 1000;
  private static final int GENERATE_LINKS_LOG_SAMPLE_RATE_DEFAULT = 100;

  private static final ImmutableList<String> ENTITY_LIST_DEFAULT_VALUE =
      ImmutableList.of(
          TextClassifier.TYPE_ADDRESS,
          TextClassifier.TYPE_EMAIL,
          TextClassifier.TYPE_PHONE,
          TextClassifier.TYPE_URL,
          TextClassifier.TYPE_DATE,
          TextClassifier.TYPE_DATE_TIME,
          TextClassifier.TYPE_FLIGHT_NUMBER);
  private static final ImmutableList<String> CONVERSATION_ACTIONS_TYPES_DEFAULT_VALUES =
      ImmutableList.of(
          ConversationAction.TYPE_TEXT_REPLY,
          ConversationAction.TYPE_CREATE_REMINDER,
          ConversationAction.TYPE_CALL_PHONE,
          ConversationAction.TYPE_OPEN_URL,
          ConversationAction.TYPE_SEND_EMAIL,
          ConversationAction.TYPE_SEND_SMS,
          ConversationAction.TYPE_TRACK_FLIGHT,
          ConversationAction.TYPE_VIEW_CALENDAR,
          ConversationAction.TYPE_VIEW_MAP,
          TYPE_ADD_CONTACT,
          TYPE_COPY);
  /**
   * < 0 : Not set. Use value from LangId model. 0 - 1: Override value in LangId model.
   *
   * @see EntityConfidence
   */
  private static final float LANG_ID_THRESHOLD_OVERRIDE_DEFAULT = -1f;

  private static final float TRANSLATE_ACTION_THRESHOLD_DEFAULT = 0.5f;

  private static final boolean USER_LANGUAGE_PROFILE_ENABLED_DEFAULT = true;
  private static final boolean TEMPLATE_INTENT_FACTORY_ENABLED_DEFAULT = true;
  private static final boolean TRANSLATE_IN_CLASSIFICATION_ENABLED_DEFAULT = true;
  private static final boolean DETECT_LANGUAGES_FROM_TEXT_ENABLED_DEFAULT = true;
  private static final boolean MODEL_DOWNLOAD_MANAGER_ENABLED_DEFAULT = false;
  private static final int MODEL_DOWNLOAD_MAX_ATTEMPTS_DEFAULT = 5;
  private static final String ANNOTATOR_URL_PREFIX_DEFAULT =
      "https://www.gstatic.com/android/text_classifier/";
  private static final String LANG_ID_URL_PREFIX_DEFAULT =
      "https://www.gstatic.com/android/text_classifier/langid/";
  private static final String ACTIONS_SUGGESTIONS_URL_PREFIX_DEFAULT =
      "https://www.gstatic.com/android/text_classifier/actions/";
  private static final String PRIMARY_ANNOTATOR_URL_SUFFIX_DEFAULT = "";
  private static final String PRIMARY_LANG_ID_URL_SUFFIX_DEFAULT = "";
  private static final String PRIMARY_ACTIONS_SUGGESTIONS_URL_SUFFIX_DEFAULT = "";
  private static final float[] LANG_ID_CONTEXT_SETTINGS_DEFAULT = new float[] {20f, 1.0f, 0.4f};

  @VisibleForTesting
  interface IDeviceConfig {
    default int getInt(@NonNull String namespace, @NonNull String name, @NonNull int defaultValue) {
      return defaultValue;
    }

    default float getFloat(
        @NonNull String namespace, @NonNull String name, @NonNull float defaultValue) {
      return defaultValue;
    }

    default String getString(
        @NonNull String namespace, @NonNull String name, @Nullable String defaultValue) {
      return defaultValue;
    }

    default boolean getBoolean(
        @NonNull String namespace, @NonNull String name, boolean defaultValue) {
      return defaultValue;
    }
  }

  private static final IDeviceConfig DEFAULT_DEVICE_CONFIG =
      new IDeviceConfig() {
        @Override
        public int getInt(
            @NonNull String namespace, @NonNull String name, @NonNull int defaultValue) {
          return DeviceConfig.getInt(namespace, name, defaultValue);
        }

        @Override
        public float getFloat(
            @NonNull String namespace, @NonNull String name, @NonNull float defaultValue) {
          return DeviceConfig.getFloat(namespace, name, defaultValue);
        }

        @Override
        public String getString(
            @NonNull String namespace, @NonNull String name, @NonNull String defaultValue) {
          return DeviceConfig.getString(namespace, name, defaultValue);
        }

        @Override
        public boolean getBoolean(
            @NonNull String namespace, @NonNull String name, @NonNull boolean defaultValue) {
          return DeviceConfig.getBoolean(namespace, name, defaultValue);
        }
      };

  private final IDeviceConfig deviceConfig;

  public TextClassifierSettings() {
    this(DEFAULT_DEVICE_CONFIG);
  }

  @VisibleForTesting
  TextClassifierSettings(IDeviceConfig deviceConfig) {
    this.deviceConfig = deviceConfig;
  }

  public int getSuggestSelectionMaxRangeLength() {
    return deviceConfig.getInt(
        NAMESPACE, SUGGEST_SELECTION_MAX_RANGE_LENGTH, SUGGEST_SELECTION_MAX_RANGE_LENGTH_DEFAULT);
  }

  public int getClassifyTextMaxRangeLength() {
    return deviceConfig.getInt(
        NAMESPACE, CLASSIFY_TEXT_MAX_RANGE_LENGTH, CLASSIFY_TEXT_MAX_RANGE_LENGTH_DEFAULT);
  }

  public int getGenerateLinksMaxTextLength() {
    return deviceConfig.getInt(
        NAMESPACE, GENERATE_LINKS_MAX_TEXT_LENGTH, GENERATE_LINKS_MAX_TEXT_LENGTH_DEFAULT);
  }

  public int getGenerateLinksLogSampleRate() {
    return deviceConfig.getInt(
        NAMESPACE, GENERATE_LINKS_LOG_SAMPLE_RATE, GENERATE_LINKS_LOG_SAMPLE_RATE_DEFAULT);
  }

  public List<String> getEntityListDefault() {
    return getDeviceConfigStringList(ENTITY_LIST_DEFAULT, ENTITY_LIST_DEFAULT_VALUE);
  }

  public List<String> getEntityListNotEditable() {
    return getDeviceConfigStringList(ENTITY_LIST_NOT_EDITABLE, ENTITY_LIST_DEFAULT_VALUE);
  }

  public List<String> getEntityListEditable() {
    return getDeviceConfigStringList(ENTITY_LIST_EDITABLE, ENTITY_LIST_DEFAULT_VALUE);
  }

  public List<String> getInAppConversationActionTypes() {
    return getDeviceConfigStringList(
        IN_APP_CONVERSATION_ACTION_TYPES_DEFAULT, CONVERSATION_ACTIONS_TYPES_DEFAULT_VALUES);
  }

  public List<String> getNotificationConversationActionTypes() {
    return getDeviceConfigStringList(
        NOTIFICATION_CONVERSATION_ACTION_TYPES_DEFAULT, CONVERSATION_ACTIONS_TYPES_DEFAULT_VALUES);
  }

  public float getLangIdThresholdOverride() {
    return deviceConfig.getFloat(
        NAMESPACE, LANG_ID_THRESHOLD_OVERRIDE, LANG_ID_THRESHOLD_OVERRIDE_DEFAULT);
  }

  public float getTranslateActionThreshold() {
    return deviceConfig.getFloat(
        NAMESPACE, TRANSLATE_ACTION_THRESHOLD, TRANSLATE_ACTION_THRESHOLD_DEFAULT);
  }

  public boolean isUserLanguageProfileEnabled() {
    return deviceConfig.getBoolean(
        NAMESPACE, USER_LANGUAGE_PROFILE_ENABLED, USER_LANGUAGE_PROFILE_ENABLED_DEFAULT);
  }

  public boolean isTemplateIntentFactoryEnabled() {
    return deviceConfig.getBoolean(
        NAMESPACE, TEMPLATE_INTENT_FACTORY_ENABLED, TEMPLATE_INTENT_FACTORY_ENABLED_DEFAULT);
  }

  public boolean isTranslateInClassificationEnabled() {
    return deviceConfig.getBoolean(
        NAMESPACE,
        TRANSLATE_IN_CLASSIFICATION_ENABLED,
        TRANSLATE_IN_CLASSIFICATION_ENABLED_DEFAULT);
  }

  public boolean isDetectLanguagesFromTextEnabled() {
    return deviceConfig.getBoolean(
        NAMESPACE, DETECT_LANGUAGES_FROM_TEXT_ENABLED, DETECT_LANGUAGES_FROM_TEXT_ENABLED_DEFAULT);
  }

  public float[] getLangIdContextSettings() {
    return getDeviceConfigFloatArray(LANG_ID_CONTEXT_SETTINGS, LANG_ID_CONTEXT_SETTINGS_DEFAULT);
  }

  public boolean isModelDownloadManagerEnabled() {
    return deviceConfig.getBoolean(
        NAMESPACE, MODEL_DOWNLOAD_MANAGER_ENABLED, MODEL_DOWNLOAD_MANAGER_ENABLED_DEFAULT);
  }

  public int getModelDownloadMaxAttempts() {
    return deviceConfig.getInt(
        NAMESPACE, MODEL_DOWNLOAD_MAX_ATTEMPTS, MODEL_DOWNLOAD_MAX_ATTEMPTS_DEFAULT);
  }

  public String getModelURLPrefix(@ModelType.ModelTypeDef String modelType) {
    switch (modelType) {
      case ModelType.ANNOTATOR:
        return deviceConfig.getString(
            NAMESPACE, ANNOTATOR_URL_PREFIX, ANNOTATOR_URL_PREFIX_DEFAULT);
      case ModelType.LANG_ID:
        return deviceConfig.getString(NAMESPACE, LANG_ID_URL_PREFIX, LANG_ID_URL_PREFIX_DEFAULT);
      case ModelType.ACTIONS_SUGGESTIONS:
        return deviceConfig.getString(
            NAMESPACE, ACTIONS_SUGGESTIONS_URL_PREFIX, ACTIONS_SUGGESTIONS_URL_PREFIX_DEFAULT);
      default:
        return "";
    }
  }

  public String getPrimaryModelURLSuffix(@ModelType.ModelTypeDef String modelType) {
    switch (modelType) {
      case ModelType.ANNOTATOR:
        return deviceConfig.getString(
            NAMESPACE, PRIMARY_ANNOTATOR_URL_SUFFIX, PRIMARY_ANNOTATOR_URL_SUFFIX_DEFAULT);
      case ModelType.LANG_ID:
        return deviceConfig.getString(
            NAMESPACE, PRIMARY_LANG_ID_URL_SUFFIX, PRIMARY_LANG_ID_URL_SUFFIX_DEFAULT);
      case ModelType.ACTIONS_SUGGESTIONS:
        return deviceConfig.getString(
            NAMESPACE,
            PRIMARY_ACTIONS_SUGGESTIONS_URL_SUFFIX,
            PRIMARY_ACTIONS_SUGGESTIONS_URL_SUFFIX_DEFAULT);
      default:
        return "";
    }
  }

  void dump(IndentingPrintWriter pw) {
    pw.println("TextClassifierSettings:");
    pw.increaseIndent();
    pw.printPair("classify_text_max_range_length", getClassifyTextMaxRangeLength());
    pw.printPair("detect_language_from_text_enabled", isDetectLanguagesFromTextEnabled());
    pw.printPair("entity_list_default", getEntityListDefault());
    pw.printPair("entity_list_editable", getEntityListEditable());
    pw.printPair("entity_list_not_editable", getEntityListNotEditable());
    pw.printPair("generate_links_log_sample_rate", getGenerateLinksLogSampleRate());
    pw.printPair("generate_links_max_text_length", getGenerateLinksMaxTextLength());
    pw.printPair("in_app_conversation_action_types_default", getInAppConversationActionTypes());
    pw.printPair("lang_id_context_settings", Arrays.toString(getLangIdContextSettings()));
    pw.printPair("lang_id_threshold_override", getLangIdThresholdOverride());
    pw.printPair("translate_action_threshold", getTranslateActionThreshold());
    pw.printPair(
        "notification_conversation_action_types_default", getNotificationConversationActionTypes());
    pw.printPair("suggest_selection_max_range_length", getSuggestSelectionMaxRangeLength());
    pw.printPair("user_language_profile_enabled", isUserLanguageProfileEnabled());
    pw.printPair("template_intent_factory_enabled", isTemplateIntentFactoryEnabled());
    pw.printPair("translate_in_classification_enabled", isTranslateInClassificationEnabled());
    pw.printPair("model_download_manager_enabled", isModelDownloadManagerEnabled());
    pw.printPair("model_download_max_attempts", getModelDownloadMaxAttempts());
    pw.printPair("annotator_url_prefix", getModelURLPrefix(ModelType.ANNOTATOR));
    pw.printPair("lang_id_url_prefix", getModelURLPrefix(ModelType.LANG_ID));
    pw.printPair(
        "actions_suggestions_url_prefix", getModelURLPrefix(ModelType.ACTIONS_SUGGESTIONS));
    pw.decreaseIndent();
    pw.printPair("primary_annotator_url_suffix", getPrimaryModelURLSuffix(ModelType.ANNOTATOR));
    pw.printPair("primary_lang_id_url_suffix", getPrimaryModelURLSuffix(ModelType.LANG_ID));
    pw.printPair(
        "primary_actions_suggestions_url_suffix",
        getPrimaryModelURLSuffix(ModelType.ACTIONS_SUGGESTIONS));
    pw.decreaseIndent();
  }

  private List<String> getDeviceConfigStringList(String key, List<String> defaultValue) {
    return parse(deviceConfig.getString(NAMESPACE, key, null), defaultValue);
  }

  private float[] getDeviceConfigFloatArray(String key, float[] defaultValue) {
    return parse(deviceConfig.getString(NAMESPACE, key, null), defaultValue);
  }

  private static List<String> parse(@Nullable String listStr, List<String> defaultValue) {
    if (listStr != null) {
      return Collections.unmodifiableList(Arrays.asList(listStr.split(DELIMITER)));
    }
    return defaultValue;
  }

  private static float[] parse(@Nullable String arrayStr, float[] defaultValue) {
    if (arrayStr != null) {
      final List<String> split = Splitter.onPattern(DELIMITER).splitToList(arrayStr);
      if (split.size() != defaultValue.length) {
        return defaultValue;
      }
      final float[] result = new float[split.size()];
      for (int i = 0; i < split.size(); i++) {
        try {
          result[i] = Float.parseFloat(split.get(i));
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
