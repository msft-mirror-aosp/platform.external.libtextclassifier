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
 * limitations under the License
 */

package com.android.textclassifier;

import static com.google.common.truth.Truth.assertWithMessage;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class TextClassificationConstantsTest {

    private static final float EPSILON = 0.0001f;

    @Test
    public void testLoadFromString_defaultValues() {
        final TextClassificationConstants constants = new TextClassificationConstants();

        assertWithMessage("suggest_selection_max_range_length")
                .that(constants.getSuggestSelectionMaxRangeLength())
                .isEqualTo(10 * 1000);
        assertWithMessage("classify_text_max_range_length")
                .that(constants.getClassifyTextMaxRangeLength())
                .isEqualTo(10 * 1000);
        assertWithMessage("generate_links_max_text_length")
                .that(constants.getGenerateLinksMaxTextLength())
                .isEqualTo(100 * 1000);
        //        assertWithMessage("generate_links_log_sample_rate")
        //                .that(constants.getGenerateLinksLogSampleRate()).isEqualTo(100);
        assertWithMessage("entity_list_default")
                .that(constants.getEntityListDefault())
                .containsExactly("address", "email", "url", "phone", "date", "datetime", "flight");
        assertWithMessage("entity_list_not_editable")
                .that(constants.getEntityListNotEditable())
                .containsExactly("address", "email", "url", "phone", "date", "datetime", "flight");
        assertWithMessage("entity_list_editable")
                .that(constants.getEntityListEditable())
                .containsExactly("address", "email", "url", "phone", "date", "datetime", "flight");
        assertWithMessage("in_app_conversation_action_types_default")
                .that(constants.getInAppConversationActionTypes())
                .containsExactly(
                        "text_reply",
                        "create_reminder",
                        "call_phone",
                        "open_url",
                        "send_email",
                        "send_sms",
                        "track_flight",
                        "view_calendar",
                        "view_map",
                        "add_contact",
                        "copy");
        assertWithMessage("notification_conversation_action_types_default")
                .that(constants.getNotificationConversationActionTypes())
                .containsExactly(
                        "text_reply",
                        "create_reminder",
                        "call_phone",
                        "open_url",
                        "send_email",
                        "send_sms",
                        "track_flight",
                        "view_calendar",
                        "view_map",
                        "add_contact",
                        "copy");
        assertWithMessage("lang_id_threshold_override")
                .that(constants.getLangIdThresholdOverride())
                .isWithin(EPSILON)
                .of(-1f);
        Assert.assertArrayEquals(
                "lang_id_context_settings",
                constants.getLangIdContextSettings(),
                new float[] {20, 1, 0.4f},
                EPSILON);
        assertWithMessage("detect_language_from_text_enabled")
                .that(constants.isDetectLanguagesFromTextEnabled())
                .isTrue();
        assertWithMessage("template_intent_factory_enabled")
                .that(constants.isTemplateIntentFactoryEnabled())
                .isTrue();
        assertWithMessage("translate_in_classification_enabled")
                .that(constants.isTranslateInClassificationEnabled())
                .isTrue();
    }
}
