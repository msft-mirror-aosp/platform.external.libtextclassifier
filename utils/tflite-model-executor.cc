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

#include "utils/tflite-model-executor.h"

#include "tensorflow/lite/kernels/register.h"
#include "utils/base/logging.h"

// Forward declaration of custom TensorFlow Lite ops for registration.
namespace tflite {
namespace ops {
namespace builtin {
TfLiteRegistration* Register_DIV();
TfLiteRegistration* Register_FULLY_CONNECTED();
TfLiteRegistration* Register_SOFTMAX();  // TODO(smillius): remove.
}  // namespace builtin
}  // namespace ops
}  // namespace tflite

void RegisterSelectedOps(::tflite::MutableOpResolver* resolver) {
  resolver->AddBuiltin(::tflite::BuiltinOperator_FULLY_CONNECTED,
                       ::tflite::ops::builtin::Register_FULLY_CONNECTED());
}

namespace libtextclassifier3 {

inline std::unique_ptr<tflite::OpResolver> BuildOpResolver() {
#ifdef TC3_USE_SELECTIVE_REGISTRATION
  std::unique_ptr<tflite::MutableOpResolver> resolver(
      new tflite::MutableOpResolver);
  resolver->AddBuiltin(tflite::BuiltinOperator_DIV,
                       tflite::ops::builtin::Register_DIV());
  resolver->AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                       tflite::ops::builtin::Register_FULLY_CONNECTED());
  resolver->AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                       tflite::ops::builtin::Register_SOFTMAX());
  RegisterSelectedOps(resolver.get());
#else
  std::unique_ptr<tflite::ops::builtin::BuiltinOpResolver> resolver(
      new tflite::ops::builtin::BuiltinOpResolver);
#endif
  return std::unique_ptr<tflite::OpResolver>(std::move(resolver));
}

std::unique_ptr<const tflite::FlatBufferModel> TfLiteModelFromModelSpec(
    const tflite::Model* model_spec) {
  std::unique_ptr<const tflite::FlatBufferModel> model(
      tflite::FlatBufferModel::BuildFromModel(model_spec));
  if (!model || !model->initialized()) {
    TC3_LOG(ERROR) << "Could not build TFLite model from a model spec.";
    return nullptr;
  }
  return model;
}

std::unique_ptr<const tflite::FlatBufferModel> TfLiteModelFromBuffer(
    const flatbuffers::Vector<uint8_t>* model_spec_buffer) {
  const tflite::Model* model =
      flatbuffers::GetRoot<tflite::Model>(model_spec_buffer->data());
  flatbuffers::Verifier verifier(model_spec_buffer->data(),
                                 model_spec_buffer->Length());
  if (!model->Verify(verifier)) {
    return nullptr;
  }
  return TfLiteModelFromModelSpec(model);
}

TfLiteModelExecutor::TfLiteModelExecutor(
    std::unique_ptr<const tflite::FlatBufferModel> model)
    : model_(std::move(model)), resolver_(BuildOpResolver()) {}

std::unique_ptr<tflite::Interpreter> TfLiteModelExecutor::CreateInterpreter()
    const {
  std::unique_ptr<tflite::Interpreter> interpreter;
  tflite::InterpreterBuilder(*model_, *resolver_)(&interpreter);
  return interpreter;
}

template <>
void TfLiteModelExecutor::SetInput(const int input_index,
                                   const std::vector<std::string>& input_data,
                                   tflite::Interpreter* interpreter) const {
  tflite::DynamicBuffer buf;
  for (const std::string& s : input_data) {
    buf.AddString(s.data(), s.length());
  }
  // TODO(b/120230709): Use WriteToTensorAsVector() instead, once available in
  // AOSP.
  buf.WriteToTensor(interpreter->tensor(interpreter->inputs()[input_index]));
}

template <>
std::vector<tflite::StringRef> TfLiteModelExecutor::Output(
    const int output_index, tflite::Interpreter* interpreter) const {
  const TfLiteTensor* output_tensor =
      interpreter->tensor(interpreter->outputs()[output_index]);
  const int num_strings = tflite::GetStringCount(output_tensor);
  std::vector<tflite::StringRef> output(num_strings);
  for (int i = 0; i < num_strings; i++) {
    output[i] = tflite::GetString(output_tensor, i);
  }
  return output;
}

template <>
std::vector<std::string> TfLiteModelExecutor::Output(
    const int output_index, tflite::Interpreter* interpreter) const {
  std::vector<std::string> output;
  for (const tflite::StringRef& s :
       Output<tflite::StringRef>(output_index, interpreter)) {
    output.push_back(std::string(s.str, s.len));
  }
  return output;
}

}  // namespace libtextclassifier3
