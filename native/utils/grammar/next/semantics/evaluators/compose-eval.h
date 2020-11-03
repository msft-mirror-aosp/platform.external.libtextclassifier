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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVALUATORS_COMPOSE_EVAL_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVALUATORS_COMPOSE_EVAL_H_

#include "utils/base/arena.h"
#include "utils/flatbuffers/mutable.h"
#include "utils/grammar/next/semantics/eval-context.h"
#include "utils/grammar/next/semantics/evaluator.h"
#include "utils/grammar/next/semantics/expression_generated.h"
#include "utils/grammar/next/semantics/value.h"

namespace libtextclassifier3::grammar::next {

// Combines arguments to a result type.
class ComposeEvaluator : public SemanticExpressionEvaluator {
 public:
  explicit ComposeEvaluator(const SemanticExpressionEvaluator* composer,
                            const reflection::Schema* semantic_values_schema)
      : composer_(composer), semantic_value_builder_(semantic_values_schema) {}

  StatusOr<const SemanticValue*> Apply(const EvalContext& context,
                                       const SemanticExpression* expression,
                                       UnsafeArena* arena) const override;

 private:
  const SemanticExpressionEvaluator* composer_;
  const MutableFlatbufferBuilder semantic_value_builder_;
};

}  // namespace libtextclassifier3::grammar::next

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVALUATORS_COMPOSE_EVAL_H_
