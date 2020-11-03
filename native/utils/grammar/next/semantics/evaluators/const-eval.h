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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVALUATORS_CONST_EVAL_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVALUATORS_CONST_EVAL_H_

#include "utils/base/arena.h"
#include "utils/grammar/next/semantics/eval-context.h"
#include "utils/grammar/next/semantics/evaluator.h"
#include "utils/grammar/next/semantics/expression_generated.h"
#include "utils/grammar/next/semantics/value.h"

namespace libtextclassifier3::grammar::next {

// Returns a constant value of a given type.
class ConstEvaluator : public SemanticExpressionEvaluator {
 public:
  explicit ConstEvaluator(const reflection::Schema* semantic_values_schema)
      : semantic_values_schema_(semantic_values_schema) {}

  StatusOr<const SemanticValue*> Apply(const EvalContext&,
                                       const SemanticExpression* expression,
                                       UnsafeArena* arena) const override {
    TC3_DCHECK_EQ(expression->expression_type(),
                  SemanticExpression_::Expression_ConstValueExpression);
    const ConstValueExpression* const_value_expression =
        expression->expression_as_ConstValueExpression();
    const reflection::BaseType base_type =
        static_cast<reflection::BaseType>(const_value_expression->base_type());
    const StringPiece data = StringPiece(
        reinterpret_cast<const char*>(const_value_expression->value()->data()),
        const_value_expression->value()->size());
    switch (base_type) {
      case reflection::BaseType::Bool: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<bool>(data.data()));
      }
      case reflection::BaseType::Byte: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<int8>(data.data()));
      }
      case reflection::BaseType::UByte: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<uint8>(data.data()));
      }
      case reflection::BaseType::Short: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<int16>(data.data()));
      }
      case reflection::BaseType::UShort: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<uint16>(data.data()));
      }
      case reflection::BaseType::Int: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<int32>(data.data()));
      }
      case reflection::BaseType::UInt: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<uint32>(data.data()));
      }
      case reflection::BaseType::Long: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<int64>(data.data()));
      }
      case reflection::BaseType::ULong: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<uint64>(data.data()));
      }
      case reflection::BaseType::Float: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<float>(data.data()));
      }
      case reflection::BaseType::Double: {
        return arena->AllocAndInit<SemanticValue>(
            flatbuffers::ReadScalar<double>(data.data()));
      }
      case reflection::BaseType::String: {
        return arena->AllocAndInit<SemanticValue>(data);
      }
      case reflection::BaseType::Obj: {
        const int type_id = const_value_expression->type();
        if (type_id < 0 ||
            type_id >= semantic_values_schema_->objects()->size()) {
          return Status(StatusCode::INVALID_ARGUMENT, "Invalid type.");
        }
        return arena->AllocAndInit<SemanticValue>(
            semantic_values_schema_->objects()->Get(
                const_value_expression->type()),
            data);
      }
      default:
        return Status(StatusCode::INVALID_ARGUMENT, "Unsupported type.");
    }
  }

 private:
  const reflection::Schema* semantic_values_schema_;
};

}  // namespace libtextclassifier3::grammar::next

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVALUATORS_CONST_EVAL_H_
