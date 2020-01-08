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

// PrefixVarint is an integer encoding method that has the exact same
// compression size as Varint, but is faster to decode because all of the
// length information is encoded in the first byte.
// On a Warp 19 it can parse up to 42% faster than Varint, for the distributions
// tested below.
// On an Ilium it can parse up to 37% faster than Varint.
//
// But there are a few caveats:
// - This is fastest if both the encoder and decoder are little endian.
//   Somewhat slower versions are provided for encoding and decoding on big
//   endian machines.
// - This doesn't support backwards decoding.
//
// The PrefixVarint encoding uses a unary code in the high bits of the first
// byte to encode the total number of bytes, as follows:
// - 32bit encoding:
//     1 byte:  "0" + 7 value bits
//     2 bytes: "10" + 6 value bits
//     3 bytes: "110" + 5 value bits
//     4 bytes: "1110" + 4 value bits
//     5 bytes: "1111" + no value bits (value is in the next 4 bytes)
//
// - 64bit encoding:
//     1 byte:  "0" + 7 value bits
//     2 bytes: "10" + 6 value bits
//     3 bytes: "110" + 5 value bits
//     4 bytes: "1110" + 4 value bits
//     5 bytes: "11110" + 3 value bits
//     6 bytes: "111110" + 2 value bits
//     7 bytes: "1111110" + 1 value bits
//     8 bytes: "11111110" + no value bits (value is in the next 7 bytes)
//     9 bytes: "11111111" + no value bits (value is in the next 8 bytes)
//
// Note that 32bit and 64bit PrefixVarint encoding are same for values between
// 0 and (1<<28)-1 (i.e., upto 4 byte-encodable value).
//
// The following are benchmark results (in cycles per operation, so lower is
// better) on randomly generated sequences of values whose encodings have the
// given distribution of byte lengths.  The cycle counts include some overhead
// (1-2 cycles) for the testing loop operation.
//
// UNIFORM 2^14 means the values are randomly generated in the range [0-2^14),
// so the majority will require 2 bytes to encode.  MIXED 60:20:10:6:4, on the
// other hand, means 60% of the values encode to 1 byte, 20% to 2 bytes, and
// so on.  The MIXED 15:71:13:1.2:0.1 distribution simulates a power law with
// median value of 1024.
//
// VI is Varint, PVI is PrefixVarint.  In both cases, Parse32Inline was used.
//
// Warp 19 (Opteron):
//                            Encode     Parse       Skip
// Byte Len Dist              VI  PVI    VI  PVI    VI  PVI
// UNIFORM 2^7              12.2  9.9   3.4  3.3   3.2  3.2
// UNIFORM 2^14             18.2 14.0   8.8  6.0   5.4  6.4
// UNIFORM 2^21             18.1 15.1  13.0  9.7   6.7  9.5
// UNIFORM 2^28             18.9 14.9  15.4 12.1   9.8 10.7
// UNIFORM 2^31             23.6 19.3  20.1 14.9  12.7 10.7
// MIXED 50:50:0:0:0        19.4 19.8  15.0 12.7  11.8 12.6
// MIXED 20:20:20:20:20     28.2 27.3  24.9 21.8  20.7 18.8
// MIXED 60:20:10:6:4       23.5 23.3  29.7 17.3  16.7 16.3
// MIXED 80:12:5:2:1        16.5 16.3  11.6  9.9   9.7  9.6
// MIXED 90:7:2:1:0         12.9 12.9   8.2  6.2   6.1  6.1
// MIXED 15:71:13:1.2:0.1   18.9 19.2  13.8 11.2  11.0 11.8
//
// Ilium:
//                            Encode     Parse       Skip
// Byte Len Dist              VI  PVI    VI  PVI    VI  PVI
// UNIFORM 2^7              10.2  8.7   3.1  3.1   2.9  2.1
// UNIFORM 2^14             15.8 13.2   7.1  4.5   4.2  3.4
// UNIFORM 2^21             15.6 14.1  10.1  6.6   5.4  5.7
// UNIFORM 2^28             18.1 15.2  12.7  8.8   7.3  8.3
// UNIFORM 2^31             21.8 16.5  17.9 13.3  13.9  8.1
// MIXED 50:50:0:0:0        19.8 20.7  14.2 13.0  12.4 12.2
// MIXED 20:20:20:20:20     29.8 30.1  27.7 24.3  22.7 20.2
// MIXED 60:20:10:6:4       24.2 24.9  20.1 18.9  18.7 17.2
// MIXED 80:12:5:2:1        16.3 16.6  12.0 11.6  11.3 10.7
// MIXED 90:7:2:1:0         12.1 12.3   7.2  7.0   6.8  6.5
// MIXED 15:71:13:1.2:0.1   19.2 20.1  14.2 13.1  12.5 12.0
//

#ifndef LIBTEXTCLASSIFIER_UTILS_BASE_PREFIXVARINT_H_
#define LIBTEXTCLASSIFIER_UTILS_BASE_PREFIXVARINT_H_

#include <string>

#include "utils/base/casts.h"
#include "utils/base/endian.h"
#include "utils/base/integral_types.h"
#include "utils/base/unaligned_access.h"

namespace libtextclassifier3 {

class PrefixVarint {
 public:
  // The max bytes used to encode a uint32:
  static const int kMax32 = 5;
  static const int kMax64 = 9;

  // This decoder does not read past the encoded buffer.
  static const int kSlopBytes = 0;

  // Returns the number of bytes used to encode the given value:
  static int Length32(uint32 val);
  static int Length64(uint64 val);

  // The Encode functions could reset up to the following bytes past the last
  // encoded byte. Use the slower SafeEncode equivalent if you want the encode
  // to not use any slop bytes.
  static const int kEncode32SlopBytes = 1;
  static const int kEncode64SlopBytes = 3;

  // The safer version of the Encode functions, which don't need any slop bytes.
  static char* SafeEncode32(char* ptr, uint32 val);
  static char* SafeEncode64(char* ptr, uint64 val);
  // Inlined version:
  static char* SafeEncode32Inline(char* ptr, uint32 val);
  static char* SafeEncode64Inline(char* ptr, uint64 val);

  // Appends the encoded value to *s.
  static void Append32(std::string* s, uint32 value);
  static void Append64(std::string* s, uint64 value);

  // Parses the next value in the ptr buffer and returns the pointer advanced
  // past the end of the encoded value.
  static const char* Parse32(const char* ptr, uint32* val);
  static const char* Parse64(const char* ptr, uint64* val);
  // Use this in time-critical code:
  static const char* Parse32Inline(const char* ptr, uint32* val);
  static const char* Parse64Inline(const char* ptr, uint64* val);

 private:
  static const int kMin2Bytes = (1 << 7);
  static const int kMin3Bytes = (1 << 14);
  static const int kMin4Bytes = (1 << 21);
  static const int kMin5Bytes = (1 << 28);
  static const int64 kMin6Bytes = (1LL << 35);
  static const int64 kMin7Bytes = (1LL << 42);
  static const int64 kMin8Bytes = (1LL << 49);
  static const int64 kMin9Bytes = (1LL << 56);

  static void Append32Slow(std::string* s, uint32 value);
  static void Append64Slow(std::string* s, uint64 value);
  static const char* Parse32Fallback(uint32 code, const char* ptr, uint32* val);
  static const char* Parse64Fallback(uint64 code, const char* ptr, uint64* val);
  static const char* Parse32FallbackInline(uint32 code, const char* ptr,
                                           uint32* val);
  static const char* Parse64FallbackInline(uint64 code, const char* ptr,
                                           uint64* val);

  // Casting helpers to aid in making this code signed-char-clean.
  static uint8* MakeUnsigned(char* p) { return bit_cast<uint8*>(p); }
  static const uint8* MakeUnsigned(const char* p) {
    return bit_cast<const uint8*>(p);
  }
};

inline int PrefixVarint::Length32(uint32 val) {
  if (val < kMin2Bytes) return 1;
  if (val < kMin3Bytes) return 2;
  if (val < kMin4Bytes) return 3;
  if (val < kMin5Bytes) return 4;
  return 5;
}

inline int PrefixVarint::Length64(uint64 val) {
  if (val < kMin2Bytes) return 1;
  if (val < kMin3Bytes) return 2;
  if (val < kMin4Bytes) return 3;
  if (val < kMin5Bytes) return 4;
  if (val < kMin6Bytes) return 5;
  if (val < kMin7Bytes) return 6;
  if (val < kMin8Bytes) return 7;
  if (val < kMin9Bytes) return 8;
  return 9;
}

inline char* PrefixVarint::SafeEncode32Inline(char* p, uint32 val) {
  uint8* const ptr = MakeUnsigned(p);
  if (val < kMin2Bytes) {
    ptr[0] = val;
    return p + 1;
  } else if (val < kMin3Bytes) {
    val <<= 2;
    uint8 low = val;
    ptr[0] = (low >> 2) | 128;
    ptr[1] = val >> 8;
    return p + 2;
  } else if (val < kMin4Bytes) {
    val <<= 3;
    uint8 low = val;
    ptr[0] = (low >> 3) | 192;
    ptr[1] = val >> 8;
    ptr[2] = val >> 16;
    return p + 3;
  } else if (val < kMin5Bytes) {
    val <<= 4;
    uint8 low = val;
    ptr[0] = (low >> 4) | 224;
    ptr[1] = val >> 8;
    ptr[2] = val >> 16;
    ptr[3] = val >> 24;
    return p + 4;
  } else {
    ptr[0] = 0xff;
    ptr[1] = val;
    ptr[2] = val >> 8;
    ptr[3] = val >> 16;
    ptr[4] = val >> 24;
    return p + 5;
  }
}

inline char* PrefixVarint::SafeEncode64Inline(char* p, uint64 val) {
  uint8* const ptr = MakeUnsigned(p);
  if (val < kMin2Bytes) {
    ptr[0] = val;
    return p + 1;
  } else if (val < kMin3Bytes) {
    val <<= 2;
    uint8 low = val;
    ptr[0] = (low >> 2) | 128;
    ptr[1] = val >> 8;
    return p + 2;
  } else if (val < kMin4Bytes) {
    val <<= 3;
    uint8 low = val;
    ptr[0] = (low >> 3) | 192;
    ptr[1] = val >> 8;
    ptr[2] = val >> 16;
    return p + 3;
  } else if (val < kMin5Bytes) {
    val <<= 4;
    uint8 low = val;
    ptr[0] = (low >> 4) | 224;
    ptr[1] = val >> 8;
    ptr[2] = val >> 16;
    ptr[3] = val >> 24;
    return p + 4;
  } else if (val < kMin6Bytes) {
    val <<= 5;
    uint8 low = val;
    ptr[0] = (low >> 5) | 240;
    ptr[1] = val >> 8;
    ptr[2] = val >> 16;
    ptr[3] = val >> 24;
    ptr[4] = val >> 32;
    return p + 5;
  } else if (val < kMin7Bytes) {
    val <<= 6;
    uint8 low = val;
    ptr[0] = (low >> 6) | 248;
    ptr[1] = val >> 8;
    ptr[2] = val >> 16;
    ptr[3] = val >> 24;
    ptr[4] = val >> 32;
    ptr[5] = val >> 40;
    return p + 6;
  } else if (val < kMin8Bytes) {
    val <<= 7;
    uint8 low = val;
    ptr[0] = (low >> 7) | 252;
    ptr[1] = val >> 8;
    ptr[2] = val >> 16;
    ptr[3] = val >> 24;
    ptr[4] = val >> 32;
    ptr[5] = val >> 40;
    ptr[6] = val >> 48;
    return p + 7;
  } else if (val < kMin9Bytes) {
    ptr[0] = 254;
    ptr[1] = val;
    ptr[2] = val >> 8;
    ptr[3] = val >> 16;
    ptr[4] = val >> 24;
    ptr[5] = val >> 32;
    ptr[6] = val >> 40;
    ptr[7] = val >> 48;
    return p + 8;
  } else {
    ptr[0] = 255;
    ptr[1] = val;
    ptr[2] = val >> 8;
    ptr[3] = val >> 16;
    ptr[4] = val >> 24;
    ptr[5] = val >> 32;
    ptr[6] = val >> 40;
    ptr[7] = val >> 48;
    ptr[8] = val >> 56;
    return p + 9;
  }
}

inline void PrefixVarint::Append32(std::string* s, uint32 value) {
  // Inline the fast-path for single-character output, but fall back to the .cc
  // file for the full version. The size<capacity check is so the compiler can
  // optimize out the string resize code.
  if (value < kMin2Bytes && s->size() < s->capacity()) {
    s->push_back(static_cast<unsigned char>(value));
  } else {
    Append32Slow(s, value);
  }
}

inline void PrefixVarint::Append64(std::string* s, uint64 value) {
  // Inline the fast-path for single-character output, but fall back to the .cc
  // file for the full version. The size<capacity check is so the compiler can
  // optimize out the string resize code.
  if (value < kMin2Bytes && s->size() < s->capacity()) {
    s->push_back(static_cast<unsigned char>(value));
  } else {
    Append64Slow(s, value);
  }
}

#ifdef IS_LITTLE_ENDIAN

inline const char* PrefixVarint::Parse32(const char* p, uint32* val) {
  const uint8* const ptr = MakeUnsigned(p);
  uint32 code = *ptr;
  if (code < 128) {
    *val = code;
    return p + 1;
  } else if (code < 192) {
    uint32 v = ptr[1];
    *val = (code & 0x3f) | (v << 6);
    return p + 2;
  } else {
    return Parse32Fallback(code, p, val);
  }
}

inline const char* PrefixVarint::Parse64(const char* p, uint64* val) {
  const uint8* const ptr = MakeUnsigned(p);
  uint64 code = *ptr;
  if (code < 128) {
    *val = code;
    return p + 1;
  } else if (code < 192) {
    uint64 v = ptr[1];
    *val = (code & 0x3fLLU) | (v << 6);
    return p + 2;
  } else {
    return Parse64Fallback(code, p, val);
  }
}

inline const char* PrefixVarint::Parse32Inline(const char* p, uint32* val) {
  const uint8* const ptr = MakeUnsigned(p);
  uint32 code = *ptr;
  if (code < 128) {
    *val = code;
    return p + 1;
  } else if (code < 192) {
    uint32 v = ptr[1];
    *val = (code & 0x3f) | (v << 6);
    return p + 2;
  } else {
    return Parse32FallbackInline(code, p, val);
  }
}

inline const char* PrefixVarint::Parse64Inline(const char* p, uint64* val) {
  const uint8* const ptr = MakeUnsigned(p);
  uint64 code = *ptr;
  if (code < 128) {
    *val = code;
    return p + 1;
  } else if (code < 192) {
    uint64 v = ptr[1];
    *val = (code & 0x3f) | (v << 6);
    return p + 2;
  } else {
    return Parse64FallbackInline(code, p, val);
  }
}

// Only handles cases with 3-5 bytes
inline const char* PrefixVarint::Parse32FallbackInline(uint32 code,
                                                       const char* p,
                                                       uint32* val) {
  const uint8* const ptr = MakeUnsigned(p);
  if (code < 224) {
    uint32 v = TC3_UNALIGNED_LOAD16(ptr + 1);
    *val = (code & 0x1f) | (v << 5);
    return p + 3;
  } else if (code < 240) {
    uint32 v = ptr[3];
    v = (v << 16) | TC3_UNALIGNED_LOAD16(ptr + 1);
    *val = (code & 0xf) | (v << 4);
    return p + 4;
  } else {
    *val = TC3_UNALIGNED_LOAD32(ptr + 1);
    return p + 5;
  }
}

// Only handles cases with 3-9 bytes
inline const char* PrefixVarint::Parse64FallbackInline(uint64 code,
                                                       const char* p,
                                                       uint64* val) {
  const uint8* const ptr = MakeUnsigned(p);
  if (code < 224) {
    uint64 v = TC3_UNALIGNED_LOAD16(ptr + 1);
    *val = (code & 0x1fLLU) | (v << 5);
    return p + 3;
  } else if (code < 240) {
    uint64 v = ptr[3];
    v = (v << 16) | TC3_UNALIGNED_LOAD16(ptr + 1);
    *val = (code & 0xfLLU) | (v << 4);
    return p + 4;
  } else if (code < 248) {
    uint64 v = TC3_UNALIGNED_LOAD32(ptr + 1);
    *val = (code & 0x7LLU) | (v << 3);
    return p + 5;
  } else if (code < 252) {
    uint64 v = ptr[5];
    v = (v << 32) | TC3_UNALIGNED_LOAD32(ptr + 1);
    *val = (code & 0x3LLU) | (v << 2);
    return p + 6;
  } else if (code < 254) {
    uint64 v = TC3_UNALIGNED_LOAD16(ptr + 5);
    v = (v << 32) | TC3_UNALIGNED_LOAD32(ptr + 1);
    *val = (code & 0x1LLU) | (v << 1);
    return p + 7;
  } else if (code < 255) {
    uint64 v = TC3_UNALIGNED_LOAD64(ptr);
    *val = v >> 8;
    return p + 8;
  } else {
    *val = TC3_UNALIGNED_LOAD64(ptr + 1);
    return p + 9;
  }
}

#else  // IS_BIG_ENDIAN

// This works on big-endian machines.  Performance is 1-16% slower, depending
// on the data.
inline const char* PrefixVarint::Parse32(const char* p, uint32* val) {
  const uint8* const ptr = MakeUnsigned(p);
  uint32 code = *ptr;
  if (code < 128) {
    *val = code;
    return p + 1;
  } else if (code < 192) {
    uint32 v = ptr[1];
    *val = (code & 0x3f) | (v << 6);
    return p + 2;
  } else {
    return Parse32Fallback(code, p, val);
  }
}

inline const char* PrefixVarint::Parse64(const char* p, uint64* val) {
  const uint8* const ptr = MakeUnsigned(p);
  uint64 code = *ptr;
  if (code < 128) {
    *val = code;
    return p + 1;
  } else if (code < 192) {
    uint64 v = ptr[1];
    *val = (code & 0x3fLLU) | (v << 6);
    return p + 2;
  } else {
    return Parse64Fallback(code, p, val);
  }
}

inline const char* PrefixVarint::Parse32Inline(const char* p, uint32* val) {
  const uint8* const ptr = MakeUnsigned(p);
  uint32 code = *ptr;
  if (code < 128) {
    *val = code;
    return p + 1;
  } else if (code < 192) {
    uint32 v = ptr[1];
    *val = (code & 0x3f) | (v << 6);
    return p + 2;
  } else {
    return Parse32FallbackInline(code, p, val);
  }
}

inline const char* PrefixVarint::Parse64Inline(const char* p, uint64* val) {
  const uint8* const ptr = MakeUnsigned(p);
  uint64 code = *ptr;
  if (code < 128) {
    *val = code;
    return p + 1;
  } else if (code < 192) {
    uint64 v = ptr[1];
    *val = (code & 0x3fLLU) | (v << 6);
    return p + 2;
  } else {
    return Parse64FallbackInline(code, p, val);
  }
}

// Only handles cases with 3-5 bytes
inline const char* PrefixVarint::Parse32FallbackInline(uint32 code,
                                                       const char* p,
                                                       uint32* val) {
  const uint8* const ptr = MakeUnsigned(p);
  if (code < 224) {
    uint32 v = ptr[2];
    v = (v << 8) | ptr[1];
    *val = (code & 0x1f) | (v << 5);
    return p + 3;
  } else if (code < 240) {
    uint32 v = ptr[3];
    v = (v << 8) | ptr[2];
    v = (v << 8) | ptr[1];
    *val = (code & 0xf) | (v << 4);
    return p + 4;
  } else {
    uint32 v = ptr[4];
    v = (v << 8) | ptr[3];
    v = (v << 8) | ptr[2];
    v = (v << 8) | ptr[1];
    *val = v;
    return p + 5;
  }
}

// Only handles cases with 3-9 bytes
inline const char* PrefixVarint::Parse64FallbackInline(uint64 code,
                                                       const char* p,
                                                       uint64* val) {
  const uint8* const ptr = MakeUnsigned(p);
  if (code < 224) {
    uint64 v = ptr[2];
    v = (v << 8) | ptr[1];
    *val = (code & 0x1f) | (v << 5);
    return p + 3;
  } else if (code < 240) {
    uint64 v = ptr[3];
    v = (v << 8) | ptr[2];
    v = (v << 8) | ptr[1];
    *val = (code & 0xf) | (v << 4);
    return p + 4;
  } else if (code < 248) {
    uint64 v = ptr[4];
    v = (v << 8) | ptr[3];
    v = (v << 8) | ptr[2];
    v = (v << 8) | ptr[1];
    *val = (code & 0x7) | (v << 3);
    return p + 5;
  } else if (code < 252) {
    uint64 v = ptr[5];
    v = (v << 8) | ptr[4];
    v = (v << 8) | ptr[3];
    v = (v << 8) | ptr[2];
    v = (v << 8) | ptr[1];
    *val = (code & 0x3) | (v << 2);
    return p + 6;
  } else if (code < 254) {
    uint64 v = ptr[6];
    v = (v << 8) | ptr[5];
    v = (v << 8) | ptr[4];
    v = (v << 8) | ptr[3];
    v = (v << 8) | ptr[2];
    v = (v << 8) | ptr[1];
    *val = (code & 0x1) | (v << 1);
    return p + 7;
  } else if (code < 255) {
    uint64 v = ptr[7];
    v = (v << 8) | ptr[6];
    v = (v << 8) | ptr[5];
    v = (v << 8) | ptr[4];
    v = (v << 8) | ptr[3];
    v = (v << 8) | ptr[2];
    v = (v << 8) | ptr[1];
    *val = v;
    return p + 8;
  } else {
    uint64 v = ptr[8];
    v = (v << 8) | ptr[7];
    v = (v << 8) | ptr[6];
    v = (v << 8) | ptr[5];
    v = (v << 8) | ptr[4];
    v = (v << 8) | ptr[3];
    v = (v << 8) | ptr[2];
    v = (v << 8) | ptr[1];
    *val = v;
    return p + 9;
  }
}

#endif  // IS_LITTLE_ENDIAN

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_BASE_PREFIXVARINT_H_
