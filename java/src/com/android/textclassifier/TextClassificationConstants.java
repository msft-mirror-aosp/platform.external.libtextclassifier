/*
 * Copyright (C) 2017 The Android Open Source Project
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

import android.view.textclassifier.ConversationAction;
import android.view.textclassifier.TextClassifier;

import com.android.textclassifier.utils.IndentingPrintWriter;

import java.util.Arrays;
import java.util.List;

/**
 * TextClassifier specific settings. This is encoded as a key=value list, separated by commas.
 *
 * <p>Example of setting the values for testing.
 *
 * <p>
 *
 * <pre>
 * adb shell settings put global text_classifier_constants \
 *      model_dark_launch_enabled=true,smart_selection_enabled=true, \
 *      entity_list_default=phone:address, \
 *      lang_id_context_settings=20:1.0:0.4
 * </pre>
 *
 * <p>Settings are also available in device config. These take precedence over those in settings
 * global.
 *
 * <p>
 *
 * <pre>
 * adb shell cmd device_config put textclassifier system_textclassifier_enabled true
 * </pre>
 *
 * @see android.provider.DeviceConfig.NAMESPACE_TEXTCLASSIFIER
 */
// TODO: Rename to TextClassifierSettings.
public final class TextClassificationConstants {

    /** Whether the smart linkify feature is enabled. */
    private static final String SMART_LINKIFY_ENABLED = "smart_linkify_enabled";
    /** Whether SystemTextClassifier is enabled. */
    private static final String SYSTEM_TEXT_CLASSIFIER_ENABLED = "system_textclassifier_enabled";
    /** Whether TextClassifierImpl is enabled. */
    private static final String LOCAL_TEXT_CLASSIFIER_ENABLED = "local_textclassifier_enabled";
    /** Enable smart selection without a visible UI changes. */
    private static final String MODEL_DARK_LAUNCH_ENABLED = "model_dark_launch_enabled";

    /** Whether the smart selection feature is enabled. */
    private static final String SMART_SELECTION_ENABLED = "smart_selection_enabled";
    /** Whether the smart text share feature is enabled. */
    private static final String SMART_TEXT_SHARE_ENABLED = "smart_text_share_enabled";
    /** Whether animation for smart selection is enabled. */
    private static final String SMART_SELECT_ANIMATION_ENABLED = "smart_select_animation_enabled";
    /** Whether the user language profile feature is enabled. */
    private static final String USER_LANGUAGE_PROFILE_ENABLED = "user_language_profile_enabled";
    /** Max length of text that suggestSelection can accept. */
    private static final String SUGGEST_SELECTION_MAX_RANGE_LENGTH =
            "suggest_selection_max_range_length";
    /** Max length of text that classifyText can accept. */
    private static final String CLASSIFY_TEXT_MAX_RANGE_LENGTH = "classify_text_max_range_length";
    /** Max length of text that generateLinks can accept. */
    private static final String GENERATE_LINKS_MAX_TEXT_LENGTH = "generate_links_max_text_length";
    /** Sampling rate for generateLinks logging. */
    private static final String GENERATE_LINKS_LOG_SAMPLE_RATE = "generate_links_log_sample_rate";
    /**
     * Extra count that is added to some languages, e.g. system languages, when deducing the
     * frequent languages in {@link
     * com.android.textclassifier.ulp.LanguageProfileAnalyzer#getFrequentLanguages(int)}.
     */
    private static final String FREQUENT_LANGUAGES_BOOTSTRAPPING_COUNT =
            "frequent_languages_bootstrapping_count";

    /**
     * Default count for the language in the system settings while calculating {@code
     * LanguageProfileAnalyzer.getRecognizedLanguages()}
     */
    private static final String LANGUAGE_PROFICIENCY_BOOTSTRAPPING_COUNT =
            "language_proficiency_bootstrapping_count";

    /**
     * A colon(:) separated string that specifies the default entities types for generateLinks when
     * hint is not given.
     */
    private static final String ENTITY_LIST_DEFAULT = "entity_list_default";
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
    private static final String LANG_ID_THRESHOLD_OVERRIDE = "lang_id_threshold_override";
    /** Whether to enable {@link com.android.textclassifier.intent.TemplateIntentFactory}. */
    private static final String TEMPLATE_INTENT_FACTORY_ENABLED = "template_intent_factory_enabled";
    /** Whether to enable "translate" action in classifyText. */
    private static final String TRANSLATE_IN_CLASSIFICATION_ENABLED =
            "translate_in_classification_enabled";
    /**
     * Whether to detect the languages of the text in request by using langId for the native model.
     */
    private static final String DETECT_LANGUAGES_FROM_TEXT_ENABLED =
            "detect_languages_from_text_enabled";
    /**
     * A colon(:) separated string that specifies the configuration to use when including
     * surrounding context text in language detection queries.
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
    private static final String LANG_ID_CONTEXT_SETTINGS = "lang_id_context_settings";
    /** Default threshold to translate the language of the context the user selects */
    private static final String TRANSLATE_ACTION_THRESHOLD = "translate_action_threshold";

    // Sync this with ConversationAction.TYPE_ADD_CONTACT;
    public static final String TYPE_ADD_CONTACT = "add_contact";
    // Sync this with ConversationAction.COPY;
    public static final String TYPE_COPY = "copy";

    private static final boolean LOCAL_TEXT_CLASSIFIER_ENABLED_DEFAULT = true;
    private static final boolean SYSTEM_TEXT_CLASSIFIER_ENABLED_DEFAULT = true;
    private static final boolean MODEL_DARK_LAUNCH_ENABLED_DEFAULT = false;
    private static final boolean SMART_SELECTION_ENABLED_DEFAULT = true;
    private static final boolean SMART_TEXT_SHARE_ENABLED_DEFAULT = true;
    private static final boolean SMART_LINKIFY_ENABLED_DEFAULT = true;
    private static final boolean SMART_SELECT_ANIMATION_ENABLED_DEFAULT = true;
    private static final int SUGGEST_SELECTION_MAX_RANGE_LENGTH_DEFAULT = 10 * 1000;
    private static final int CLASSIFY_TEXT_MAX_RANGE_LENGTH_DEFAULT = 10 * 1000;
    private static final int GENERATE_LINKS_MAX_TEXT_LENGTH_DEFAULT = 100 * 1000;
    private static final int GENERATE_LINKS_LOG_SAMPLE_RATE_DEFAULT = 100;
    private static final int FREQUENT_LANGUAGES_BOOTSTRAPPING_COUNT_DEFAULT = 100;
    private static final int LANGUAGE_PROFICIENCY_BOOTSTRAPPING_COUNT_DEFAULT = 100;

    private static final List<String> ENTITY_LIST_DEFAULT_VALUE =
            Arrays.asList(
                    TextClassifier.TYPE_ADDRESS,
                    TextClassifier.TYPE_EMAIL,
                    TextClassifier.TYPE_PHONE,
                    TextClassifier.TYPE_URL,
                    TextClassifier.TYPE_DATE,
                    TextClassifier.TYPE_DATE_TIME,
                    TextClassifier.TYPE_FLIGHT_NUMBER);
    private static final List<String> CONVERSATION_ACTIONS_TYPES_DEFAULT_VALUES =
            Arrays.asList(
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
    private static final float[] LANG_ID_CONTEXT_SETTINGS_DEFAULT = new float[] {20f, 1.0f, 0.4f};

    private final ConfigParser mConfigParser;

    public TextClassificationConstants() {
        mConfigParser = new ConfigParser();
    }

    public int getSuggestSelectionMaxRangeLength() {
        return mConfigParser.getInt(
                SUGGEST_SELECTION_MAX_RANGE_LENGTH, SUGGEST_SELECTION_MAX_RANGE_LENGTH_DEFAULT);
    }

    public int getClassifyTextMaxRangeLength() {
        return mConfigParser.getInt(
                CLASSIFY_TEXT_MAX_RANGE_LENGTH, CLASSIFY_TEXT_MAX_RANGE_LENGTH_DEFAULT);
    }

    public int getGenerateLinksMaxTextLength() {
        return mConfigParser.getInt(
                GENERATE_LINKS_MAX_TEXT_LENGTH, GENERATE_LINKS_MAX_TEXT_LENGTH_DEFAULT);
    }

    public int getGenerateLinksLogSampleRate() {
        return mConfigParser.getInt(
                GENERATE_LINKS_LOG_SAMPLE_RATE, GENERATE_LINKS_LOG_SAMPLE_RATE_DEFAULT);
    }

    public int getFrequentLanguagesBootstrappingCount() {
        return mConfigParser.getInt(
                FREQUENT_LANGUAGES_BOOTSTRAPPING_COUNT,
                FREQUENT_LANGUAGES_BOOTSTRAPPING_COUNT_DEFAULT);
    }

    public int getLanguageProficiencyBootstrappingCount() {
        return mConfigParser.getInt(
                LANGUAGE_PROFICIENCY_BOOTSTRAPPING_COUNT,
                LANGUAGE_PROFICIENCY_BOOTSTRAPPING_COUNT_DEFAULT);
    }

    public List<String> getEntityListDefault() {
        return mConfigParser.getStringList(ENTITY_LIST_DEFAULT, ENTITY_LIST_DEFAULT_VALUE);
    }

    public List<String> getEntityListNotEditable() {
        return mConfigParser.getStringList(ENTITY_LIST_NOT_EDITABLE, ENTITY_LIST_DEFAULT_VALUE);
    }

    public List<String> getEntityListEditable() {
        return mConfigParser.getStringList(ENTITY_LIST_EDITABLE, ENTITY_LIST_DEFAULT_VALUE);
    }

    public List<String> getInAppConversationActionTypes() {
        return mConfigParser.getStringList(
                IN_APP_CONVERSATION_ACTION_TYPES_DEFAULT,
                CONVERSATION_ACTIONS_TYPES_DEFAULT_VALUES);
    }

    public List<String> getNotificationConversationActionTypes() {
        return mConfigParser.getStringList(
                NOTIFICATION_CONVERSATION_ACTION_TYPES_DEFAULT,
                CONVERSATION_ACTIONS_TYPES_DEFAULT_VALUES);
    }

    public float getLangIdThresholdOverride() {
        return mConfigParser.getFloat(
                LANG_ID_THRESHOLD_OVERRIDE, LANG_ID_THRESHOLD_OVERRIDE_DEFAULT);
    }

    public float getTranslateActionThreshold() {
        return mConfigParser.getFloat(
                TRANSLATE_ACTION_THRESHOLD, TRANSLATE_ACTION_THRESHOLD_DEFAULT);
    }

    public boolean isUserLanguageProfileEnabled() {
        return mConfigParser.getBoolean(
                USER_LANGUAGE_PROFILE_ENABLED, USER_LANGUAGE_PROFILE_ENABLED_DEFAULT);
    }

    public boolean isTemplateIntentFactoryEnabled() {
        return mConfigParser.getBoolean(
                TEMPLATE_INTENT_FACTORY_ENABLED, TEMPLATE_INTENT_FACTORY_ENABLED_DEFAULT);
    }

    public boolean isTranslateInClassificationEnabled() {
        return mConfigParser.getBoolean(
                TRANSLATE_IN_CLASSIFICATION_ENABLED, TRANSLATE_IN_CLASSIFICATION_ENABLED_DEFAULT);
    }

    public boolean isDetectLanguagesFromTextEnabled() {
        return mConfigParser.getBoolean(
                DETECT_LANGUAGES_FROM_TEXT_ENABLED, DETECT_LANGUAGES_FROM_TEXT_ENABLED_DEFAULT);
    }

    public float[] getLangIdContextSettings() {
        return mConfigParser.getFloatArray(
                LANG_ID_CONTEXT_SETTINGS, LANG_ID_CONTEXT_SETTINGS_DEFAULT);
    }

    void dump(IndentingPrintWriter pw) {
        pw.println("TextClassificationConstants:");
        pw.increaseIndent();
        pw.printPair("classify_text_max_range_length", getClassifyTextMaxRangeLength());
        pw.printPair("detect_language_from_text_enabled", isDetectLanguagesFromTextEnabled());
        pw.printPair("entity_list_default", getEntityListDefault());
        pw.printPair("entity_list_editable", getEntityListEditable());
        pw.printPair("entity_list_not_editable", getEntityListNotEditable());
        pw.printPair("generate_links_log_sample_rate", getGenerateLinksLogSampleRate());
        pw.printPair(
                "frequent_languages_bootstrapping_count", getFrequentLanguagesBootstrappingCount());
        pw.printPair("generate_links_max_text_length", getGenerateLinksMaxTextLength());
        pw.printPair("in_app_conversation_action_types_default", getInAppConversationActionTypes());
        pw.printPair("lang_id_context_settings", Arrays.toString(getLangIdContextSettings()));
        pw.printPair("lang_id_threshold_override", getLangIdThresholdOverride());
        pw.printPair("translate_action_threshold", getTranslateActionThreshold());
        pw.printPair(
                "notification_conversation_action_types_default",
                getNotificationConversationActionTypes());
        pw.printPair("suggest_selection_max_range_length", getSuggestSelectionMaxRangeLength());
        pw.printPair("user_language_profile_enabled", isUserLanguageProfileEnabled());
        pw.printPair("template_intent_factory_enabled", isTemplateIntentFactoryEnabled());
        pw.printPair("translate_in_classification_enabled", isTranslateInClassificationEnabled());
        pw.printPair(
                "language proficiency bootstrapping count",
                getLanguageProficiencyBootstrappingCount());
        pw.decreaseIndent();
    }
}
