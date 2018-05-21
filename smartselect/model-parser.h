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

#ifndef LIBTEXTCLASSIFIER_SMARTSELECT_MODEL_PARSER_H_
#define LIBTEXTCLASSIFIER_SMARTSELECT_MODEL_PARSER_H_

namespace libtextclassifier {

// Parse a merged model image.
bool ParseMergedModel(const void* addr, const int size,
                      const char** selection_model, int* selection_model_length,
                      const char** sharing_model, int* sharing_model_length);

}  // namespace libtextclassifier

#endif  // LIBTEXTCLASSIFIER_SMARTSELECT_MODEL_PARSER_H_
