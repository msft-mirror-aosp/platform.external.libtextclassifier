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

#ifndef LIBTEXTCLASSIFIER_TEXTCLASSIFIER_JNI_H_
#define LIBTEXTCLASSIFIER_TEXTCLASSIFIER_JNI_H_

#include <jni.h>
#include <string>

#include "smartselect/types.h"

#ifndef TC_PACKAGE_NAME
#define TC_PACKAGE_NAME android_view_textclassifier
#endif
#ifndef TC_PACKAGE_PATH
#define TC_PACKAGE_PATH "android/view/textclassifier/"
#endif

#define JNI_METHOD_PRIMITIVE(return_type, package_name, class_name, \
                             method_name)                           \
  JNIEXPORT return_type JNICALL                                     \
      Java_##package_name##_##class_name##_##method_name

// The indirection is needed to correctly expand the TC_PACKAGE_NAME macro.
#define JNI_METHOD2(return_type, package_name, class_name, method_name) \
  JNI_METHOD_PRIMITIVE(return_type, package_name, class_name, method_name)

#define JNI_METHOD(return_type, class_name, method_name) \
  JNI_METHOD2(return_type, TC_PACKAGE_NAME, class_name, method_name)

#ifdef __cplusplus
extern "C" {
#endif

// SmartSelection.
JNI_METHOD(jlong, SmartSelection, nativeNew)
(JNIEnv* env, jobject thiz, jint fd);

JNI_METHOD(jlong, SmartSelection, nativeNewFromPath)
(JNIEnv* env, jobject thiz, jstring path);

JNI_METHOD(jlong, SmartSelection, nativeNewFromAssetFileDescriptor)
(JNIEnv* env, jobject thiz, jobject afd, jlong offset, jlong size);

JNI_METHOD(jintArray, SmartSelection, nativeSuggest)
(JNIEnv* env, jobject thiz, jlong ptr, jstring context, jint selection_begin,
 jint selection_end);

JNI_METHOD(jobjectArray, SmartSelection, nativeClassifyText)
(JNIEnv* env, jobject thiz, jlong ptr, jstring context, jint selection_begin,
 jint selection_end, jint input_flags);

JNI_METHOD(jobjectArray, SmartSelection, nativeAnnotate)
(JNIEnv* env, jobject thiz, jlong ptr, jstring context);

JNI_METHOD(void, SmartSelection, nativeClose)
(JNIEnv* env, jobject thiz, jlong ptr);

JNI_METHOD(jstring, SmartSelection, nativeGetLanguage)
(JNIEnv* env, jobject clazz, jint fd);

JNI_METHOD(jint, SmartSelection, nativeGetVersion)
(JNIEnv* env, jobject clazz, jint fd);

#ifndef LIBTEXTCLASSIFIER_DISABLE_LANG_ID
// LangId.
JNI_METHOD(jlong, LangId, nativeNew)(JNIEnv* env, jobject thiz, jint fd);

JNI_METHOD(jobjectArray, LangId, nativeFindLanguages)
(JNIEnv* env, jobject thiz, jlong ptr, jstring text);

JNI_METHOD(void, LangId, nativeClose)(JNIEnv* env, jobject thiz, jlong ptr);

JNI_METHOD(int, LangId, nativeGetVersion)(JNIEnv* env, jobject clazz, jint fd);
#endif

#ifdef __cplusplus
}
#endif

namespace libtextclassifier {

// Given a utf8 string and a span expressed in Java BMP (basic multilingual
// plane) codepoints, converts it to a span expressed in utf8 codepoints.
libtextclassifier::CodepointSpan ConvertIndicesBMPToUTF8(
    const std::string& utf8_str, libtextclassifier::CodepointSpan bmp_indices);

// Given a utf8 string and a span expressed in utf8 codepoints, converts it to a
// span expressed in Java BMP (basic multilingual plane) codepoints.
libtextclassifier::CodepointSpan ConvertIndicesUTF8ToBMP(
    const std::string& utf8_str, libtextclassifier::CodepointSpan utf8_indices);

}  // namespace libtextclassifier

#endif  // LIBTEXTCLASSIFIER_TEXTCLASSIFIER_JNI_H_
