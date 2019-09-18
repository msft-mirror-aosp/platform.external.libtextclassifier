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

#import <Foundation/Foundation.h>

/// Defines utility methods for operating with Unicode in @c NSString.
/// @discussion Unicode has 1,114,112 code points ( http://en.wikipedia.org/wiki/Code_point ),
///             and multiple encodings that map these code points into code units.
///             @c NSString API exposes the string as if it were encoded in UTF-16, which makes use
///             of surrogate pairs ( http://en.wikipedia.org/wiki/UTF-16 ).
///             The methods in this category translate indices between Unicode codepoints and
///             UTF-16 unichars.
@interface NSString (Unicode)

/// Returns the number of Unicode codepoints for a string slice.
/// @param start The NSString start index.
/// @param length The number of unichar units.
/// @return The number of Unicode code points in the specified unichar range.
- (NSUInteger)tc_countChar32:(NSUInteger)start withLength:(NSUInteger)length;

/// Returns the length of the string in terms of Unicode codepoints.
/// @return The number of Unicode codepoints in this string.
- (NSUInteger)tc_codepointLength;

@end
