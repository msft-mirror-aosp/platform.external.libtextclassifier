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

#include "utils/utf8/unilib-common.h"

#include <algorithm>

namespace libtextclassifier3 {
namespace {

#define ARRAYSIZE(a) sizeof(a) / sizeof(*a)

// Derived from http://www.unicode.org/Public/UNIDATA/UnicodeData.txt
// grep -E "Ps" UnicodeData.txt | \
//   sed -rne "s/^([0-9A-Z]{4});.*(PAREN|BRACKET|BRAKCET|BRACE).*/0x\1, /p"
// IMPORTANT: entries with the same offsets in kOpeningBrackets and
//            kClosingBrackets must be counterparts.
constexpr char32 kOpeningBrackets[] = {
    0x0028, 0x005B, 0x007B, 0x0F3C, 0x2045, 0x207D, 0x208D, 0x2329, 0x2768,
    0x276A, 0x276C, 0x2770, 0x2772, 0x2774, 0x27E6, 0x27E8, 0x27EA, 0x27EC,
    0x27EE, 0x2983, 0x2985, 0x2987, 0x2989, 0x298B, 0x298D, 0x298F, 0x2991,
    0x2993, 0x2995, 0x2997, 0x29FC, 0x2E22, 0x2E24, 0x2E26, 0x2E28, 0x3008,
    0x300A, 0x300C, 0x300E, 0x3010, 0x3014, 0x3016, 0x3018, 0x301A, 0xFD3F,
    0xFE17, 0xFE35, 0xFE37, 0xFE39, 0xFE3B, 0xFE3D, 0xFE3F, 0xFE41, 0xFE43,
    0xFE47, 0xFE59, 0xFE5B, 0xFE5D, 0xFF08, 0xFF3B, 0xFF5B, 0xFF5F, 0xFF62};
constexpr int kNumOpeningBrackets = ARRAYSIZE(kOpeningBrackets);

// grep -E "Pe" UnicodeData.txt | \
//   sed -rne "s/^([0-9A-Z]{4});.*(PAREN|BRACKET|BRAKCET|BRACE).*/0x\1, /p"
constexpr char32 kClosingBrackets[] = {
    0x0029, 0x005D, 0x007D, 0x0F3D, 0x2046, 0x207E, 0x208E, 0x232A, 0x2769,
    0x276B, 0x276D, 0x2771, 0x2773, 0x2775, 0x27E7, 0x27E9, 0x27EB, 0x27ED,
    0x27EF, 0x2984, 0x2986, 0x2988, 0x298A, 0x298C, 0x298E, 0x2990, 0x2992,
    0x2994, 0x2996, 0x2998, 0x29FD, 0x2E23, 0x2E25, 0x2E27, 0x2E29, 0x3009,
    0x300B, 0x300D, 0x300F, 0x3011, 0x3015, 0x3017, 0x3019, 0x301B, 0xFD3E,
    0xFE18, 0xFE36, 0xFE38, 0xFE3A, 0xFE3C, 0xFE3E, 0xFE40, 0xFE42, 0xFE44,
    0xFE48, 0xFE5A, 0xFE5C, 0xFE5E, 0xFF09, 0xFF3D, 0xFF5D, 0xFF60, 0xFF63};
constexpr int kNumClosingBrackets = ARRAYSIZE(kClosingBrackets);

// grep -E "WS" UnicodeData.txt | sed -re "s/([0-9A-Z]+);.*/0x\1, /"
constexpr char32 kWhitespaces[] = {
    0x000C,  0x0020,  0x1680,  0x2000,  0x2001,  0x2002,  0x2003,  0x2004,
    0x2005,  0x2006,  0x2007,  0x2008,  0x2009,  0x200A,  0x2028,  0x205F,
    0x21C7,  0x21C8,  0x21C9,  0x21CA,  0x21F6,  0x2B31,  0x2B84,  0x2B85,
    0x2B86,  0x2B87,  0x2B94,  0x3000,  0x4DCC,  0x10344, 0x10347, 0x1DA0A,
    0x1DA0B, 0x1DA0C, 0x1DA0D, 0x1DA0E, 0x1DA0F, 0x1DA10, 0x1F4F0, 0x1F500,
    0x1F501, 0x1F502, 0x1F503, 0x1F504, 0x1F5D8, 0x1F5DE};
constexpr int kNumWhitespaces = ARRAYSIZE(kWhitespaces);

// grep -E "Nd" UnicodeData.txt | sed -re "s/([0-9A-Z]+);.*/0x\1, /"
// As the name suggests, these ranges are always 10 codepoints long, so we just
// store the end of the range.
constexpr char32 kDecimalDigitRangesEnd[] = {
    0x0039,  0x0669,  0x06f9,  0x07c9,  0x096f,  0x09ef,  0x0a6f,  0x0aef,
    0x0b6f,  0x0bef,  0x0c6f,  0x0cef,  0x0d6f,  0x0def,  0x0e59,  0x0ed9,
    0x0f29,  0x1049,  0x1099,  0x17e9,  0x1819,  0x194f,  0x19d9,  0x1a89,
    0x1a99,  0x1b59,  0x1bb9,  0x1c49,  0x1c59,  0xa629,  0xa8d9,  0xa909,
    0xa9d9,  0xa9f9,  0xaa59,  0xabf9,  0xff19,  0x104a9, 0x1106f, 0x110f9,
    0x1113f, 0x111d9, 0x112f9, 0x11459, 0x114d9, 0x11659, 0x116c9, 0x11739,
    0x118e9, 0x11c59, 0x11d59, 0x16a69, 0x16b59, 0x1d7ff};
constexpr int kNumDecimalDigitRangesEnd = ARRAYSIZE(kDecimalDigitRangesEnd);

// grep -E "Lu" UnicodeData.txt | sed -re "s/([0-9A-Z]+);.*/0x\1, /"
// There are three common ways in which upper/lower case codepoint ranges
// were introduced: one offs, dense ranges, and ranges that alternate between
// lower and upper case. For the sake of keeping out binary size down, we
// treat each independently.
constexpr char32 kUpperSingles[] = {
    0x01b8, 0x01bc, 0x01c4, 0x01c7, 0x01ca, 0x01f1, 0x0376, 0x037f,
    0x03cf, 0x03f4, 0x03fa, 0x10c7, 0x10cd, 0x2102, 0x2107, 0x2115,
    0x2145, 0x2183, 0x2c72, 0x2c75, 0x2cf2, 0xa7b6};
constexpr int kNumUpperSingles = ARRAYSIZE(kUpperSingles);
constexpr char32 kUpperRanges1Start[] = {
    0x0041, 0x00c0, 0x00d8, 0x0181, 0x018a, 0x018e, 0x0193, 0x0196,
    0x019c, 0x019f, 0x01b2, 0x01f7, 0x023a, 0x023d, 0x0244, 0x0389,
    0x0392, 0x03a3, 0x03d2, 0x03fd, 0x0531, 0x10a0, 0x13a0, 0x1f08,
    0x1f18, 0x1f28, 0x1f38, 0x1f48, 0x1f68, 0x1fb8, 0x1fc8, 0x1fd8,
    0x1fe8, 0x1ff8, 0x210b, 0x2110, 0x2119, 0x212b, 0x2130, 0x213e,
    0x2c00, 0x2c63, 0x2c6e, 0x2c7e, 0xa7ab, 0xa7b0};
constexpr int kNumUpperRanges1Start = ARRAYSIZE(kUpperRanges1Start);
constexpr char32 kUpperRanges1End[] = {
    0x005a, 0x00d6, 0x00de, 0x0182, 0x018b, 0x0191, 0x0194, 0x0198,
    0x019d, 0x01a0, 0x01b3, 0x01f8, 0x023b, 0x023e, 0x0246, 0x038a,
    0x03a1, 0x03ab, 0x03d4, 0x042f, 0x0556, 0x10c5, 0x13f5, 0x1f0f,
    0x1f1d, 0x1f2f, 0x1f3f, 0x1f4d, 0x1f6f, 0x1fbb, 0x1fcb, 0x1fdb,
    0x1fec, 0x1ffb, 0x210d, 0x2112, 0x211d, 0x212d, 0x2133, 0x213f,
    0x2c2e, 0x2c64, 0x2c70, 0x2c80, 0xa7ae, 0xa7b4};
constexpr int kNumUpperRanges1End = ARRAYSIZE(kUpperRanges1End);
constexpr char32 kUpperRanges2Start[] = {
    0x0100, 0x0139, 0x014a, 0x0179, 0x0184, 0x0187, 0x01a2, 0x01a7, 0x01ac,
    0x01af, 0x01b5, 0x01cd, 0x01de, 0x01f4, 0x01fa, 0x0241, 0x0248, 0x0370,
    0x0386, 0x038c, 0x038f, 0x03d8, 0x03f7, 0x0460, 0x048a, 0x04c1, 0x04d0,
    0x1e00, 0x1e9e, 0x1f59, 0x2124, 0x2c60, 0x2c67, 0x2c82, 0x2ceb, 0xa640,
    0xa680, 0xa722, 0xa732, 0xa779, 0xa77e, 0xa78b, 0xa790, 0xa796};
constexpr int kNumUpperRanges2Start = ARRAYSIZE(kUpperRanges2Start);
constexpr char32 kUpperRanges2End[] = {
    0x0136, 0x0147, 0x0178, 0x017d, 0x0186, 0x0189, 0x01a6, 0x01a9, 0x01ae,
    0x01b1, 0x01b7, 0x01db, 0x01ee, 0x01f6, 0x0232, 0x0243, 0x024e, 0x0372,
    0x0388, 0x038e, 0x0391, 0x03ee, 0x03f9, 0x0480, 0x04c0, 0x04cd, 0x052e,
    0x1e94, 0x1efe, 0x1f5f, 0x212a, 0x2c62, 0x2c6d, 0x2ce2, 0x2ced, 0xa66c,
    0xa69a, 0xa72e, 0xa76e, 0xa77d, 0xa786, 0xa78d, 0xa792, 0xa7aa};
constexpr int kNumUpperRanges2End = ARRAYSIZE(kUpperRanges2End);

// grep -E "Ll" UnicodeData.txt | sed -re "s/([0-9A-Z]+);.*/0x\1, /"
constexpr char32 kLowerSingles[] = {
    0x00b5, 0x0188, 0x0192, 0x0195, 0x019e, 0x01b0, 0x01c6, 0x01c9,
    0x01f0, 0x023c, 0x0242, 0x0377, 0x0390, 0x03f5, 0x03f8, 0x1fbe,
    0x210a, 0x2113, 0x212f, 0x2134, 0x2139, 0x214e, 0x2184, 0x2c61,
    0x2ce4, 0x2cf3, 0x2d27, 0x2d2d, 0xa7af, 0xa7c3, 0xa7fa, 0x1d7cb};
constexpr int kNumLowerSingles = ARRAYSIZE(kLowerSingles);
constexpr char32 kLowerRanges1Start[] = {
    0x0061,  0x00df,  0x00f8,  0x017f,  0x018c,  0x0199,  0x01b9,  0x01bd,
    0x0234,  0x023f,  0x0250,  0x0295,  0x037b,  0x03ac,  0x03d0,  0x03d5,
    0x03f0,  0x03fb,  0x0430,  0x0560,  0x10d0,  0x10fd,  0x13f8,  0x1c80,
    0x1d00,  0x1d6b,  0x1d79,  0x1e96,  0x1f00,  0x1f10,  0x1f20,  0x1f30,
    0x1f40,  0x1f50,  0x1f60,  0x1f70,  0x1f80,  0x1f90,  0x1fa0,  0x1fb0,
    0x1fb6,  0x1fc2,  0x1fc6,  0x1fd0,  0x1fd6,  0x1fe0,  0x1ff2,  0x1ff6,
    0x210e,  0x213c,  0x2146,  0x2c30,  0x2c65,  0x2c77,  0x2d00,  0xa730,
    0xa772,  0xa794,  0xab30,  0xab60,  0xab70,  0xfb00,  0xfb13,  0xff41,
    0x10428, 0x104d8, 0x10cc0, 0x118c0, 0x16e60, 0x1d41a, 0x1d44e, 0x1d456,
    0x1d482, 0x1d4b6, 0x1d4be, 0x1d4c5, 0x1d4ea, 0x1d51e, 0x1d552, 0x1d586,
    0x1d5ba, 0x1d5ee, 0x1d622, 0x1d656, 0x1d68a, 0x1d6c2, 0x1d6dc, 0x1d6fc,
    0x1d716, 0x1d736, 0x1d750, 0x1d770, 0x1d78a, 0x1d7aa, 0x1d7c4, 0x1e922};
constexpr int kNumLowerRanges1Start = ARRAYSIZE(kLowerRanges1Start);
constexpr char32 kLowerRanges1End[] = {
    0x007a,  0x00f6,  0x00ff,  0x0180,  0x018d,  0x019b,  0x01ba,  0x01bf,
    0x0239,  0x0240,  0x0293,  0x02af,  0x037d,  0x03ce,  0x03d1,  0x03d7,
    0x03f3,  0x03fc,  0x045f,  0x0588,  0x10fa,  0x10ff,  0x13fd,  0x1c88,
    0x1d2b,  0x1d77,  0x1d9a,  0x1e9d,  0x1f07,  0x1f15,  0x1f27,  0x1f37,
    0x1f45,  0x1f57,  0x1f67,  0x1f7d,  0x1f87,  0x1f97,  0x1fa7,  0x1fb4,
    0x1fb7,  0x1fc4,  0x1fc7,  0x1fd3,  0x1fd7,  0x1fe7,  0x1ff4,  0x1ff7,
    0x210f,  0x213d,  0x2149,  0x2c5e,  0x2c66,  0x2c7b,  0x2d25,  0xa731,
    0xa778,  0xa795,  0xab5a,  0xab67,  0xabbf,  0xfb06,  0xfb17,  0xff5a,
    0x1044f, 0x104fb, 0x10cf2, 0x118df, 0x16e7f, 0x1d433, 0x1d454, 0x1d467,
    0x1d49b, 0x1d4b9, 0x1d4c3, 0x1d4cf, 0x1d503, 0x1d537, 0x1d56b, 0x1d59f,
    0x1d5d3, 0x1d607, 0x1d63b, 0x1d66f, 0x1d6a5, 0x1d6da, 0x1d6e1, 0x1d714,
    0x1d71b, 0x1d74e, 0x1d755, 0x1d788, 0x1d78f, 0x1d7c2, 0x1d7c9, 0x1e943};
constexpr int kNumLowerRanges1End = ARRAYSIZE(kLowerRanges1End);
constexpr char32 kLowerRanges2Start[] = {
    0x0101, 0x0138, 0x0149, 0x017a, 0x0183, 0x01a1, 0x01a8, 0x01ab,
    0x01b4, 0x01cc, 0x01dd, 0x01f3, 0x01f9, 0x0247, 0x0371, 0x03d9,
    0x0461, 0x048b, 0x04c2, 0x04cf, 0x1e01, 0x1e9f, 0x2c68, 0x2c71,
    0x2c74, 0x2c81, 0x2cec, 0xa641, 0xa681, 0xa723, 0xa733, 0xa77a,
    0xa77f, 0xa78c, 0xa791, 0xa797, 0xa7b5, 0x1d4bb};
constexpr int kNumLowerRanges2Start = ARRAYSIZE(kLowerRanges2Start);
constexpr char32 kLowerRanges2End[] = {
    0x0137, 0x0148, 0x0177, 0x017e, 0x0185, 0x01a5, 0x01aa, 0x01ad,
    0x01b6, 0x01dc, 0x01ef, 0x01f5, 0x0233, 0x024f, 0x0373, 0x03ef,
    0x0481, 0x04bf, 0x04ce, 0x052f, 0x1e95, 0x1eff, 0x2c6c, 0x2c73,
    0x2c76, 0x2ce3, 0x2cee, 0xa66d, 0xa69b, 0xa72f, 0xa771, 0xa77c,
    0xa787, 0xa78e, 0xa793, 0xa7a9, 0xa7bf, 0x1d4bd};
constexpr int kNumLowerRanges2End = ARRAYSIZE(kLowerRanges2End);

// grep -E "Lu" UnicodeData.txt | \
//   sed -rne "s/^([0-9A-Z]+);.*;([0-9A-Z]+);$/(0x\1, 0x\2), /p"
// We have two strategies for mapping from upper to lower case. We have single
// character lookups that do not follow a pattern, and ranges for which there
// is a constant codepoint shift.
// Note that these ranges ignore anything that's not an upper case character,
// so when applied to a non-uppercase character the result is incorrect.
constexpr int kToLowerSingles[] = {
    0x0130, 0x0178, 0x0181, 0x0186, 0x018b, 0x018e, 0x018f, 0x0190, 0x0191,
    0x0194, 0x0196, 0x0197, 0x0198, 0x019c, 0x019d, 0x019f, 0x01a6, 0x01a9,
    0x01ae, 0x01b7, 0x01f6, 0x01f7, 0x0220, 0x023a, 0x023d, 0x023e, 0x0243,
    0x0244, 0x0245, 0x037f, 0x0386, 0x038c, 0x03cf, 0x03f4, 0x03f9, 0x04c0,
    0x1e9e, 0x1fec, 0x2126, 0x212a, 0x212b, 0x2132, 0x2183, 0x2c60, 0x2c62,
    0x2c63, 0x2c64, 0x2c6d, 0x2c6e, 0x2c6f, 0x2c70, 0xa77d, 0xa78d, 0xa7aa,
    0xa7ab, 0xa7ac, 0xa7ad, 0xa7ae, 0xa7b0, 0xa7b1, 0xa7b2, 0xa7b3};
constexpr int kNumToLowerSingles = ARRAYSIZE(kToLowerSingles);
constexpr int kToUpperSingles[] = {
    0x0069, 0x00ff, 0x0253, 0x0254, 0x018c, 0x01dd, 0x0259, 0x025b, 0x0192,
    0x0263, 0x0269, 0x0268, 0x0199, 0x026f, 0x0272, 0x0275, 0x0280, 0x0283,
    0x0288, 0x0292, 0x0195, 0x01bf, 0x019e, 0x2c65, 0x019a, 0x2c66, 0x0180,
    0x0289, 0x028c, 0x03f3, 0x03ac, 0x03cc, 0x03d7, 0x03b8, 0x03f2, 0x04cf,
    0x00df, 0x1fe5, 0x03c9, 0x006b, 0x00e5, 0x214e, 0x2184, 0x2c61, 0x026b,
    0x1d7d, 0x027d, 0x0251, 0x0271, 0x0250, 0x0252, 0x1d79, 0x0265, 0x0266,
    0x025c, 0x0261, 0x026c, 0x026a, 0x029e, 0x0287, 0x029d, 0xab53};
constexpr int kNumToUpperSingles = ARRAYSIZE(kToUpperSingles);
constexpr int kToLowerRangesStart[] = {
    0x0041, 0x0100, 0x0189, 0x01a0, 0x01b1, 0x01b3, 0x0388,  0x038e,  0x0391,
    0x03d8, 0x03fd, 0x0400, 0x0410, 0x0460, 0x0531, 0x10a0,  0x13a0,  0x13f0,
    0x1e00, 0x1f08, 0x1fba, 0x1fc8, 0x1fd8, 0x1fda, 0x1fe8,  0x1fea,  0x1ff8,
    0x1ffa, 0x2c00, 0x2c67, 0x2c7e, 0x2c80, 0xff21, 0x10400, 0x10c80, 0x118a0};
constexpr int kNumToLowerRangesStart = ARRAYSIZE(kToLowerRangesStart);
constexpr int kToLowerRangesEnd[] = {
    0x00de, 0x0187, 0x019f, 0x01af, 0x01b2, 0x0386, 0x038c,  0x038f,  0x03cf,
    0x03fa, 0x03ff, 0x040f, 0x042f, 0x052e, 0x0556, 0x10cd,  0x13ef,  0x13f5,
    0x1efe, 0x1fb9, 0x1fbb, 0x1fcb, 0x1fd9, 0x1fdb, 0x1fe9,  0x1fec,  0x1ff9,
    0x2183, 0x2c64, 0x2c75, 0x2c7f, 0xa7b6, 0xff3a, 0x104d3, 0x10cb2, 0x118bf};
constexpr int kNumToLowerRangesEnd = ARRAYSIZE(kToLowerRangesEnd);
constexpr int kToLowerRangesOffsets[] = {
    32, 1,    205,  1,    217,   1, 37,     63, 32,  1,   -130, 80,
    32, 1,    48,   7264, 38864, 8, 1,      -8, -74, -86, -8,   -100,
    -8, -112, -128, -126, 48,    1, -10815, 1,  32,  40,  64,   32};
constexpr int kNumToLowerRangesOffsets = ARRAYSIZE(kToLowerRangesOffsets);
constexpr int kToUpperRangesStart[] = {
    0x0061, 0x0101, 0x01a1, 0x01b4, 0x023f, 0x0256, 0x028a,  0x037b,  0x03ad,
    0x03b1, 0x03cd, 0x03d9, 0x0430, 0x0450, 0x0461, 0x0561,  0x13f8,  0x1e01,
    0x1f00, 0x1f70, 0x1f72, 0x1f76, 0x1f78, 0x1f7a, 0x1f7c,  0x1fd0,  0x1fe0,
    0x2c30, 0x2c68, 0x2c81, 0x2d00, 0xab70, 0xff41, 0x10428, 0x10cc0, 0x118c0};
constexpr int kNumToUpperRangesStart = ARRAYSIZE(kToUpperRangesStart);
constexpr int kToUpperRangesEnd[] = {
    0x00fe, 0x0188, 0x01b0, 0x0387, 0x0240, 0x026c, 0x028b,  0x037d,  0x03b0,
    0x03ef, 0x03ce, 0x03fb, 0x044f, 0x045f, 0x052f, 0x0586,  0x13fd,  0x1eff,
    0x1fb1, 0x1f71, 0x1f75, 0x1f77, 0x1f79, 0x1f7c, 0x2105,  0x1fd1,  0x1fe1,
    0x2c94, 0x2c76, 0xa7b7, 0x2d2d, 0xabbf, 0xff5a, 0x104fb, 0x10cf2, 0x118df};
constexpr int kNumToUpperRangesEnd = ARRAYSIZE(kToUpperRangesEnd);
constexpr int kToUpperRangesOffsets[]{
    -32, -1,  -1, -1,  10815, -205, -217,  130,    -37, -32, -63, -1,
    -32, -80, -1, -48, -8,    -1,   8,     74,     86,  100, 128, 112,
    126, 8,   8,  -48, -1,    -1,   -7264, -38864, -32, -40, -64, -32};
constexpr int kNumToUpperRangesOffsets = ARRAYSIZE(kToUpperRangesOffsets);

#undef ARRAYSIZE

static_assert(kNumOpeningBrackets == kNumClosingBrackets,
              "mismatching number of opening and closing brackets");
static_assert(kNumLowerRanges1Start == kNumLowerRanges1End,
              "number of uppercase stride 1 range starts/ends doesn't match");
static_assert(kNumLowerRanges2Start == kNumLowerRanges2End,
              "number of uppercase stride 2 range starts/ends doesn't match");
static_assert(kNumUpperRanges1Start == kNumUpperRanges1End,
              "number of uppercase stride 1 range starts/ends doesn't match");
static_assert(kNumUpperRanges2Start == kNumUpperRanges2End,
              "number of uppercase stride 2 range starts/ends doesn't match");
static_assert(kNumToLowerSingles == kNumToUpperSingles,
              "number of to lower and upper singles doesn't match");
static_assert(kNumToLowerRangesStart == kNumToLowerRangesEnd,
              "mismatching number of range starts/ends for to lower ranges");
static_assert(kNumToLowerRangesStart == kNumToLowerRangesOffsets,
              "number of to lower ranges and offsets doesn't match");
static_assert(kNumToUpperRangesStart == kNumToUpperRangesEnd,
              "mismatching number of range starts/ends for to upper ranges");
static_assert(kNumToUpperRangesStart == kNumToUpperRangesOffsets,
              "number of to upper ranges and offsets doesn't match");

constexpr int kNoMatch = -1;

// Returns the index of the element in the array that matched the given
// codepoint, or kNoMatch if the element didn't exist.
// The input array must be in sorted order.
int GetMatchIndex(const char32* array, int array_length, char32 c) {
  const char32* end = array + array_length;
  const auto find_it = std::lower_bound(array, end, c);
  if (find_it != end && *find_it == c) {
    return find_it - array;
  } else {
    return kNoMatch;
  }
}

// Returns the index of the range in the array that overlapped the given
// codepoint, or kNoMatch if no such range existed.
// The input array must be in sorted order.
int GetOverlappingRangeIndex(const char32* arr, int arr_length,
                             int range_length, char32 c) {
  const char32* end = arr + arr_length;
  const auto find_it = std::lower_bound(arr, end, c);
  if (find_it == end) {
    return kNoMatch;
  }
  // The end is inclusive, we so subtract one less than the range length.
  const char32 range_end = *find_it;
  const char32 range_start = range_end - (range_length - 1);
  if (c < range_start || range_end < c) {
    return kNoMatch;
  } else {
    return find_it - arr;
  }
}

// As above, but with explicit codepoint start and end indices for the range.
// The input array must be in sorted order.
int GetOverlappingRangeIndex(const char32* start_arr, const char32* end_arr,
                             int arr_length, int stride, char32 c) {
  const char32* end_arr_end = end_arr + arr_length;
  const auto find_it = std::lower_bound(end_arr, end_arr_end, c);
  if (find_it == end_arr_end) {
    return kNoMatch;
  }
  // Find the corresponding start.
  const int range_index = find_it - end_arr;
  const char32 range_start = start_arr[range_index];
  const char32 range_end = *find_it;
  if (c < range_start || range_end < c) {
    return kNoMatch;
  }
  if ((c - range_start) % stride == 0) {
    return range_index;
  } else {
    return kNoMatch;
  }
}

}  // anonymous namespace

bool IsOpeningBracket(char32 codepoint) {
  return GetMatchIndex(kOpeningBrackets, kNumOpeningBrackets, codepoint) >= 0;
}

bool IsClosingBracket(char32 codepoint) {
  return GetMatchIndex(kClosingBrackets, kNumClosingBrackets, codepoint) >= 0;
}

bool IsWhitespace(char32 codepoint) {
  return GetMatchIndex(kWhitespaces, kNumWhitespaces, codepoint) >= 0;
}

bool IsDigit(char32 codepoint) {
  return GetOverlappingRangeIndex(kDecimalDigitRangesEnd,
                                  kNumDecimalDigitRangesEnd,
                                  /*range_length=*/10, codepoint) >= 0;
}

bool IsLower(char32 codepoint) {
  if (GetMatchIndex(kLowerSingles, kNumLowerSingles, codepoint) >= 0) {
    return true;
  } else if (GetOverlappingRangeIndex(kLowerRanges1Start, kLowerRanges1End,
                                      kNumLowerRanges1Start, /*stride=*/1,
                                      codepoint) >= 0) {
    return true;
  } else if (GetOverlappingRangeIndex(kLowerRanges2Start, kLowerRanges2End,
                                      kNumLowerRanges2Start, /*stride=*/2,
                                      codepoint) >= 0) {
    return true;
  } else {
    return false;
  }
}

bool IsUpper(char32 codepoint) {
  if (GetMatchIndex(kUpperSingles, kNumUpperSingles, codepoint) >= 0) {
    return true;
  } else if (GetOverlappingRangeIndex(kUpperRanges1Start, kUpperRanges1End,
                                      kNumUpperRanges1Start, /*stride=*/1,
                                      codepoint) >= 0) {
    return true;
  } else if (GetOverlappingRangeIndex(kUpperRanges2Start, kUpperRanges2End,
                                      kNumUpperRanges2Start, /*stride=*/2,
                                      codepoint) >= 0) {
    return true;
  } else {
    return false;
  }
}

char32 ToLower(char32 codepoint) {
  // Make sure we still produce output even if the method is called for a
  // codepoint that's not an uppercase character.
  if (!IsUpper(codepoint)) {
    return codepoint;
  }
  const int singles_idx =
      GetMatchIndex(kToLowerSingles, kNumToLowerSingles, codepoint);
  if (singles_idx >= 0) {
    return kToUpperSingles[singles_idx];
  }
  const int ranges_idx =
      GetOverlappingRangeIndex(kToLowerRangesStart, kToLowerRangesEnd,
                               kNumToLowerRangesStart, /*stride=*/1, codepoint);
  if (ranges_idx >= 0) {
    return codepoint + kToLowerRangesOffsets[ranges_idx];
  }
  return codepoint;
}

char32 ToUpper(char32 codepoint) {
  // Make sure we still produce output even if the method is called for a
  // codepoint that's not an uppercase character.
  if (!IsLower(codepoint)) {
    return codepoint;
  }
  const int singles_idx =
      GetMatchIndex(kToUpperSingles, kNumToUpperSingles, codepoint);
  if (singles_idx >= 0) {
    return kToLowerSingles[singles_idx];
  }
  const int ranges_idx =
      GetOverlappingRangeIndex(kToUpperRangesStart, kToUpperRangesEnd,
                               kNumToUpperRangesStart, /*stride=*/1, codepoint);
  if (ranges_idx >= 0) {
    return codepoint + kToUpperRangesOffsets[ranges_idx];
  }
  return codepoint;
}

char32 GetPairedBracket(char32 codepoint) {
  const int open_offset =
      GetMatchIndex(kOpeningBrackets, kNumOpeningBrackets, codepoint);
  if (open_offset >= 0) {
    return kClosingBrackets[open_offset];
  }
  const int close_offset =
      GetMatchIndex(kClosingBrackets, kNumClosingBrackets, codepoint);
  if (close_offset >= 0) {
    return kOpeningBrackets[close_offset];
  }
  return codepoint;
}

}  // namespace libtextclassifier3
