
#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_TESTING_EQUALS_PROTO_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_TESTING_EQUALS_PROTO_H_

#include "net/proto2/compat/public/message_lite.h"  // IWYU pragma: export
#include "gmock/gmock.h"              // IWYU pragma: export

#if defined(__ANDROID__) || defined(__APPLE__)
namespace libtextclassifier3 {
namespace portable_equals_proto {
MATCHER_P(EqualsProto, other, "Compare MessageLite by serialized string") {
  return ::testing::ExplainMatchResult(::testing::Eq(other.SerializeAsString()),
                                       arg.SerializeAsString(),
                                       result_listener);
}  // MATCHER_P
}  // namespace portable_equals_proto
}  // namespace libtextclassifier3
#else
namespace libtextclassifier3 {
namespace portable_equals_proto {
// Leverage the powerful matcher when available, for human readable
// differences.
using ::testing::EqualsProto;
}  // namespace portable_equals_proto
}  // namespace libtextclassifier3
#endif  // defined(__ANDROID__) || defined(__APPLE__)

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_TESTING_EQUALS_PROTO_H_
