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

#include "model-executor.h"

#include "quantization.h"
#include "util/base/logging.h"

namespace libtextclassifier2 {
namespace internal {
bool FromModelSpec(const tflite::Model* model_spec,
                   std::unique_ptr<const tflite::FlatBufferModel>* model) {
  *model = tflite::FlatBufferModel::BuildFromModel(model_spec);
  if (!(*model) || !(*model)->initialized()) {
    TC_LOG(ERROR) << "Could not build TFLite model from a model spec. ";
    return false;
  }
  return true;
}
}  // namespace internal

std::unique_ptr<tflite::Interpreter> ModelExecutor::CreateInterpreter() const {
  std::unique_ptr<tflite::Interpreter> interpreter;
  tflite::InterpreterBuilder(*model_, builtins_)(&interpreter);
  return interpreter;
}

TFLiteEmbeddingExecutor::TFLiteEmbeddingExecutor(
    const tflite::Model* model_spec, const int embedding_size,
    const int quantization_bits)
    : quantization_bits_(quantization_bits),
      output_embedding_size_(embedding_size) {
  internal::FromModelSpec(model_spec, &model_);
  tflite::InterpreterBuilder(*model_, builtins_)(&interpreter_);
  if (!interpreter_) {
    TC_LOG(ERROR) << "Could not build TFLite interpreter for embeddings.";
    return;
  }

  if (interpreter_->tensors_size() != 2) {
    return;
  }
  embeddings_ = interpreter_->tensor(0);
  if (embeddings_->dims->size != 2) {
    return;
  }
  num_buckets_ = embeddings_->dims->data[0];
  scales_ = interpreter_->tensor(1);
  if (scales_->dims->size != 2 || scales_->dims->data[0] != num_buckets_ ||
      scales_->dims->data[1] != 1) {
    return;
  }
  bytes_per_embedding_ = embeddings_->dims->data[1];
  if (!CheckQuantizationParams(bytes_per_embedding_, quantization_bits_,
                               output_embedding_size_)) {
    TC_LOG(ERROR) << "Mismatch in quantization parameters.";
    return;
  }

  initialized_ = true;
}

bool TFLiteEmbeddingExecutor::AddEmbedding(
    const TensorView<int>& sparse_features, float* dest, int dest_size) const {
  if (!initialized_ || dest_size != output_embedding_size_) {
    TC_LOG(ERROR) << "Mismatching dest_size and output_embedding_size: "
                  << dest_size << " " << output_embedding_size_;
    return false;
  }
  const int num_sparse_features = sparse_features.size();
  for (int i = 0; i < num_sparse_features; ++i) {
    const int bucket_id = sparse_features.data()[i];
    if (bucket_id >= num_buckets_) {
      return false;
    }

    if (!DequantizeAdd(scales_->data.f, embeddings_->data.uint8,
                       bytes_per_embedding_, num_sparse_features,
                       quantization_bits_, bucket_id, dest, dest_size)) {
      return false;
    }
  }
  return true;
}

TensorView<float> ComputeLogitsHelper(const int input_index_features,
                                      const int output_index_logits,
                                      const TensorView<float>& features,
                                      tflite::Interpreter* interpreter) {
  if (!interpreter) {
    return TensorView<float>::Invalid();
  }
  interpreter->ResizeInputTensor(input_index_features, features.shape());
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    TC_VLOG(1) << "Allocation failed.";
    return TensorView<float>::Invalid();
  }

  TfLiteTensor* features_tensor =
      interpreter->tensor(interpreter->inputs()[input_index_features]);
  int size = 1;
  for (int i = 0; i < features_tensor->dims->size; ++i) {
    size *= features_tensor->dims->data[i];
  }
  features.copy_to(features_tensor->data.f, size);

  if (interpreter->Invoke() != kTfLiteOk) {
    TC_VLOG(1) << "Interpreter failed.";
    return TensorView<float>::Invalid();
  }

  TfLiteTensor* logits_tensor =
      interpreter->tensor(interpreter->outputs()[output_index_logits]);

  std::vector<int> output_shape(logits_tensor->dims->size);
  for (int i = 0; i < logits_tensor->dims->size; ++i) {
    output_shape[i] = logits_tensor->dims->data[i];
  }

  return TensorView<float>(logits_tensor->data.f, output_shape);
}

}  // namespace libtextclassifier2
