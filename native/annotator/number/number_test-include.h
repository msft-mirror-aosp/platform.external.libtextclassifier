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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_NUMBER_NUMBER_TEST_INCLUDE_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_NUMBER_NUMBER_TEST_INCLUDE_H_

#include "annotator/number/number.h"
#include "gtest/gtest.h"

// Include the version of UniLib depending on the macro.
#if defined TC3_UNILIB_ICU
#include "utils/utf8/unilib-icu.h"

#define TC3_TESTING_CREATE_UNILIB_INSTANCE(VAR) VAR()

#elif defined TC3_UNILIB_JAVAICU
#include <jni.h>

#include "utils/utf8/unilib-javaicu.h"

extern JNIEnv* g_jenv;

#define TC3_TESTING_CREATE_UNILIB_INSTANCE(VAR) VAR(JniCache::Create(g_jenv))

#elif defined TC3_UNILIB_APPLE
#include "utils/utf8/unilib-apple.h"

#define TC3_TESTING_CREATE_UNILIB_INSTANCE(VAR) VAR()

#else

#error Unsupported configuration.

#endif

namespace libtextclassifier3 {
namespace test_internal {

class NumberAnnotatorTest : public ::testing::Test {
 protected:
  NumberAnnotatorTest()
      : TC3_TESTING_CREATE_UNILIB_INSTANCE(unilib_),
        number_annotator_(TestingNumberAnnotatorOptions(), &unilib_) {}

  const NumberAnnotatorOptions* TestingNumberAnnotatorOptions();

  UniLib unilib_;
  NumberAnnotator number_annotator_;
};

}  // namespace test_internal
}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_NUMBER_NUMBER_TEST_INCLUDE_H_
