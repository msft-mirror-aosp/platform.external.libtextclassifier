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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVALUATORS_SPAN_EVAL_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVALUATORS_SPAN_EVAL_H_

#include "annotator/types.h"
#include "utils/base/arena.h"
#include "utils/base/statusor.h"
#include "utils/grammar/next/semantics/eval-context.h"
#include "utils/grammar/next/semantics/evaluator.h"
#include "utils/grammar/next/semantics/expression_generated.h"
#include "utils/grammar/next/semantics/value.h"

namespace libtextclassifier3::grammar::next {

// Returns a value lifted from a parse tree.
class SpanAsStringEvaluator : public SemanticExpressionEvaluator {
 public:
  StatusOr<const SemanticValue*> Apply(const EvalContext& context,
                                       const SemanticExpression* expression,
                                       UnsafeArena* arena) const override {
    TC3_DCHECK_EQ(expression->expression_type(),
                  SemanticExpression_::Expression_SpanAsStringExpression);
    return arena->AllocAndInit<SemanticValue>(
        context.text_context->Span(context.parse_tree->codepoint_span));
  }
};

}  // namespace libtextclassifier3::grammar::next

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_NEXT_SEMANTICS_EVALUATORS_SPAN_EVAL_H_
