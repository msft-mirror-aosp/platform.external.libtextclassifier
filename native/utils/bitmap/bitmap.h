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

#ifndef LIBTEXTCLASSIFIER_UTILS_BITMAP_BITMAP_H_
#define LIBTEXTCLASSIFIER_UTILS_BITMAP_BITMAP_H_

#include <algorithm>
#include <climits>
#include <ostream>

#include "utils/base/integral_types.h"
#include "utils/base/logging.h"

namespace libtextclassifier3 {

template <typename W>
void SetBit(W* map, size_t index, bool value) {
  static constexpr size_t kIntBits = CHAR_BIT * sizeof(W);
  // This is written in such a way that our current compiler generates
  // a conditional move instead of a conditional branch, which is data
  // dependent and unpredictable.  Branch mis-prediction is much more
  // expensive than cost of a conditional move.
  const W bit = W{1} << (index & (kIntBits - 1));
  const W old_value = map[index / kIntBits];
  const W new_value = value ? old_value | bit : old_value & ~bit;
  map[index / kIntBits] = new_value;
}

template <typename W>
bool GetBit(const W* map, size_t index) {
  static constexpr size_t kIntBits = CHAR_BIT * sizeof(W);
  return map[index / kIntBits] & (W{1} << (index & (kIntBits - 1)));
}

namespace internal {
template <typename W>
class BasicBitmap {
 public:
  using size_type = size_t;
  using Word = W;  // packed bit internal storage type.

  // Allocates a new bitmap with size bits set to the value fill.
  BasicBitmap(size_type size, bool fill) : size_(size), alloc_(true) {
    map_ = std::allocator<Word>().allocate(array_size());
    SetAll(fill);
  }

  explicit BasicBitmap(size_type size) : BasicBitmap(size, false) {}

  // Borrows a reference to a region of memory that is the caller's
  // responsibility to manage for the life of the Bitmap. The map is expected
  // to have enough memory to store size bits.
  BasicBitmap(Word* map, size_type size)
      : map_(map), size_(size), alloc_(false) {}

  // Default constructor: creates a bitmap with zero bits.
  BasicBitmap() : size_(0), alloc_(true) {
    map_ = std::allocator<Word>().allocate(array_size());
  }

  BasicBitmap(const BasicBitmap& src);

  // Assigns this Bitmap to the values of the src Bitmap.
  // This includes pointing to the same underlying map_ if the src Bitmap
  // does not allocate its own.
  BasicBitmap& operator=(const BasicBitmap& src);

  // Destructor : clean up if we allocated
  ~BasicBitmap() {
    if (alloc_) {
      std::allocator<Word>().deallocate(map_, array_size());
    }
  }

  // Resizes the bitmap.
  // If size < bits(), the extra bits will be discarded.
  // If size > bits(), the extra bits will be filled with the fill value.
  void Resize(size_type size, bool fill = false);

  // ACCESSORS
  size_type bits() const { return size_; }
  size_type array_size() const { return RequiredArraySize(bits()); }

  // Gets an entry of the internal map. Requires array_index < array_size()
  Word GetMapElement(size_type array_index) const {
    CHECK_LT(array_index, array_size());
    return map_[array_index];
  }

  // Gets an entry of the internal map. Requires array_index < array_size()
  // Also performs masking to insure no bits >= bits().
  Word GetMaskedMapElement(size_type array_index) const {
    return (array_index == array_size() - 1)
               ? map_[array_size() - 1] & HighOrderMapElementMask()
               : map_[array_index];
  }

  // Sets an element of the internal map. Requires array_index < array_size()
  void SetMapElement(size_type array_index, Word value) {
    CHECK_LT(array_index, array_size());
    map_[array_index] = value;
  }

  // The highest order element in map_ will have some meaningless bits
  // (with undefined values) if bits() is not a multiple of
  // kIntBits. If you & HighOrderMapElementMask with the high order
  // element, you will be left with only the valid, defined bits (the
  // others will be 0)
  Word HighOrderMapElementMask() const {
    return (size_ == 0) ? 0 : (~W{0}) >> (-size_ & (kIntBits - 1));
  }

  bool Get(size_type index) const {
    TC3_DCHECK_LT(index, size_);
    return GetBit(map_, index);
  }

  // Returns true if all bits are unset
  bool IsAllZeroes() const {
    return std::all_of(map_, map_ + array_size() - 1,
                       [](Word w) { return w == W{0}; }) &&
           (map_[array_size() - 1] & HighOrderMapElementMask()) == W{0};
  }

  // Returns true if all bits are set
  bool IsAllOnes() const {
    return std::all_of(map_, map_ + array_size() - 1,
                       [](Word w) { return w == ~W{0}; }) &&
           ((~map_[array_size() - 1]) & HighOrderMapElementMask()) == W{0};
  }

  void Set(size_type index, bool value) {
    TC3_DCHECK_LT(index, size_);
    SetBit(map_, index, value);
  }

  void Toggle(size_type index) {
    TC3_DCHECK_LT(index, size_);
    map_[index / kIntBits] ^= (W{1} << (index & (kIntBits - 1)));
  }

  // Sets all the bits to true or false
  void SetAll(bool value) {
    std::fill(map_, map_ + array_size(), value ? ~W{0} : W{0});
  }

  // Clears all bits in the bitmap
  void Clear() { SetAll(false); }

  // Sets a range of bits (begin inclusive, end exclusive) to true or false
  void SetRange(size_type begin, size_type end, bool value);

  // Sets "this" to be the union of "this" and "other". The bitmaps do
  // not have to be the same size. If other is smaller, all the higher
  // order bits are assumed to be 0. The size of "this" is never
  // changed by this operation (higher order bits in other are
  // ignored). Note this make Union *not* commutative -- it matters
  // which Bitmap is this and which is other
  void Union(const BasicBitmap& other);

  // Sets "this" to be the intersection of "this" and "other". The
  // bitmaps do not have to be the same size. If other is smaller, all
  // the higher order bits are assumed to be 0. The size of this is
  // never changed by this operation (higher order bits in other are
  // ignored)
  void Intersection(const BasicBitmap& other);

  // Returns true if "this" and "other" have any bits set in common.
  bool IsIntersectionNonEmpty(const BasicBitmap& other) const;

  // Sets "this" to be the "~" (Complement) of "this".
  void Complement() {
    std::transform(map_, map_ + array_size(), map_, [](Word w) { return ~w; });
  }

  // Sets "this" to be the set of bits in "this" but not in "other"
  // REQUIRES: "bits() == other.bits()" (i.e. the bitmaps are the same size)
  void Difference(const BasicBitmap& other) {
    TC3_CHECK_EQ(bits(), other.bits());
    std::transform(map_, map_ + array_size(), other.map_, map_,
                   [](Word a, Word b) { return a & ~b; });
  }

  // Sets "this" to be the set of bits which is set in either "this" or "other",
  // but not both.
  // REQUIRES: "bits() == other.bits()" (i.e. the bitmaps are the same size)
  void ExclusiveOr(const BasicBitmap& other) {
    TC3_CHECK_EQ(bits(), other.bits());
    std::transform(map_, map_ + array_size(), other.map_, map_,
                   [](Word a, Word b) { return a ^ b; });
  }

  // Return true if any bit between begin inclusive and end exclusive
  // is set.  0 <= begin <= end <= bits() is required.
  bool TestRange(size_type begin, size_type end) const;

  // Return true if both Bitmaps are of equal length and have the same
  // value.
  bool IsEqual(const BasicBitmap& other) const {
    return (bits() == other.bits()) &&
           ((array_size() < 1) ||
            std::equal(map_, map_ + array_size() - 1, other.map_)) &&
           ((HighOrderMapElementMask() & other.map_[array_size() - 1]) ==
            (HighOrderMapElementMask() & map_[array_size() - 1]));
  }

  // Return true is this bitmap is a subset of another bitmap in terms of
  // the positions of 1s. That is, 0110 is a subset of 1110.
  // REQUIRES: "bits() == other.bits()" (i.e. the bitmaps are the same size)
  bool IsSubsetOf(const BasicBitmap& other) const;

  // Returns 0 if the two bitmaps are equal.  Returns a negative number if the
  // this bitmap is less than other, and a positive number otherwise.
  //
  // The relation we use is the natural relation defined by assigning an integer
  // to each bitmap:
  //
  // int(bitmap) = b_0 + 2 * b_1 + ... + 2^k * b_k
  //
  // Then for our comparison function:
  //
  // if int(b1) != int(b2), then b1 is less than b2 if int(b1) < int(b2),
  // and b2 is less than b1 otherwise.
  //
  // if int(b1) == int(b2), then we compare the numbers of bits in b1 and b2.
  // If b1 has strictly fewer bits, then b1 is less than b2 (same for b2).
  // If b1 and b2 have the same number of bits, then they are equal and we
  // return 0.
  int CompareTo(const BasicBitmap& other) const;

  // return number of allocated words required for a bitmap of size num_bits
  // minimum size is 1
  static constexpr size_t RequiredArraySize(size_type num_bits) {
    return num_bits == 0 ? 1 : (num_bits - 1) / kIntBits + 1;
  }

 private:
  // The same semantics as CompareTo, except that we have the invariant that
  // first has at least as many bits as second.
  static int CompareToHelper(const BasicBitmap& first,
                             const BasicBitmap& second);

  static constexpr unsigned Log2(unsigned n, unsigned p = 0) {
    return (n <= 1) ? p : Log2(n / 2, p + 1);
  }

  // NOTE: we make assumptions throughout the code that kIntBits is a power of
  // 2, so that we can use shift and mask instead of division and modulo.
  static constexpr int kIntBits = CHAR_BIT * sizeof(Word);  // bits in a Word
  static constexpr int kLogIntBits = Log2(kIntBits, 0);
  Word* map_;       // the bitmap
  size_type size_;  // the upper bound of the bitmap
  bool alloc_;      // whether or not *we* allocated the memory
};
}  // namespace internal


class Bitmap : public libtextclassifier3::internal::BasicBitmap<uint32> {
 public:
  using internal::BasicBitmap<uint32>::BasicBitmap;
};

namespace internal {
template <typename W>
BasicBitmap<W>::BasicBitmap(const BasicBitmap& src)
    : size_(src.size_), alloc_(src.alloc_) {
  static_assert(((kIntBits & (kIntBits - 1)) == 0), "kIntBits not power of 2");
  if (alloc_) {
    map_ = std::allocator<Word>().allocate(array_size());
    std::copy(src.map_, src.map_ + array_size(), map_);
  } else {
    map_ = src.map_;
  }
}

template <typename W>
void BasicBitmap<W>::Resize(size_type size, bool fill) {
  const size_type old_size = size_;
  const size_t new_array_size = RequiredArraySize(size);
  if (new_array_size != array_size()) {
    Word* new_map = std::allocator<Word>().allocate(new_array_size);
    std::copy(map_, map_ + std::min<size_t>(new_array_size, array_size()),
              new_map);
    if (alloc_) {
      std::allocator<Word>().deallocate(map_, array_size());
    }
    map_ = new_map;
    alloc_ = true;
  }
  size_ = size;
  if (old_size < size_) {
    SetRange(old_size, size_, fill);
  }
}

template <typename W>
BasicBitmap<W>& BasicBitmap<W>::operator=(const BasicBitmap<W>& src) {
  if (this != &src) {
    if (alloc_ && array_size() != src.array_size()) {
      std::allocator<Word>().deallocate(map_, array_size());
      map_ = std::allocator<Word>().allocate(src.array_size());
    }
    size_ = src.size_;
    if (src.alloc_) {
      if (!alloc_) {
        map_ = std::allocator<Word>().allocate(src.array_size());
      }
      std::copy(src.map_, src.map_ + src.array_size(), map_);
      alloc_ = true;
    } else {
      if (alloc_) {
        std::allocator<Word>().deallocate(map_, array_size());
      }
      map_ = src.map_;
      alloc_ = false;
    }
  }
  return *this;
}

// Return true if any bit between begin inclusive and end exclusive
// is set.  0 <= begin <= end <= bits() is required.
template <typename W>
bool BasicBitmap<W>::TestRange(size_type begin, size_type end) const {
  // Return false immediately if the range is empty.
  if (begin == end) {
    return false;
  }
  // Calculate the indices of the words containing the first and last bits,
  // along with the positions of the bits within those words.
  size_t i = begin / kIntBits;
  size_t j = begin & (kIntBits - 1);
  size_t ilast = (end - 1) / kIntBits;
  size_t jlast = (end - 1) & (kIntBits - 1);
  // If the range spans multiple words, discard the extraneous bits of the
  // first word by shifting to the right, and then test the remaining bits.
  if (i < ilast) {
    if (map_[i++] >> j) {
      return true;
    }
    j = 0;

    // Test each of the "middle" words that lies completely within the range.
    while (i < ilast) {
      if (map_[i++]) {
        return true;
      }
    }
  }

  // Test the portion of the last word that lies within the range. (This logic
  // also handles the case where the entire range lies within a single word.)
  const Word mask = (((W{1} << 1) << (jlast - j)) - 1) << j;
  return (map_[ilast] & mask) != W{0};
}

template <typename W>
bool BasicBitmap<W>::IsSubsetOf(const BasicBitmap& other) const {
  TC3_CHECK_EQ(bits(), other.bits());
  Word* mp = map_;
  Word* endp = mp + array_size() - 1;
  Word* op = other.map_;
  // A is a subset of B if A - B = {}, that is A & ~B = {}
  for (; mp != endp; ++mp, ++op)
    if (*mp & ~*op) return false;
  return (*mp & ~*op & HighOrderMapElementMask()) == W{0};
}

// Same semantics as CompareTo, except that we have the invariant that first
// has at least as many bits as second.
template <typename W>
int BasicBitmap<W>::CompareToHelper(const BasicBitmap<W>& first,
                                    const BasicBitmap<W>& second) {
  // Checks if the high order bits in first that are not in second are set.  If
  // any of these are set, then first is greater than second, and we return a
  // positive value.
  if (first.TestRange(second.bits(), first.bits())) {
    return 1;
  }

  // We use unsigned integer comparison to compare the bitmaps.  We need to
  // handle the high order bits in a special case (since there may be undefined
  // bits for the element representing the highest order bits) and then we
  // can do direct integer comparison.
  size_t index = second.array_size() - 1;
  Word left = first.map_[index] & second.HighOrderMapElementMask();
  Word right = second.map_[index] & second.HighOrderMapElementMask();
  if (left != right) {
    return left < right ? -1 : 1;
  }
  while (index > 0) {
    --index;
    left = first.map_[index];
    right = second.map_[index];
    if (left != right) {
      return left < right ? -1 : 1;
    }
  }
  // Now we have reached the end, all common bits are equal, and all bits that
  // are only in the longer list are 0.  We return 1 if the first bitmap is
  // strictly larger, and 0 if the bitmaps are of equal size.
  if (first.bits() == second.bits()) {
    return 0;
  } else {
    return 1;
  }
}

template <typename W>
int BasicBitmap<W>::CompareTo(const BasicBitmap<W>& other) const {
  if (bits() > other.bits()) {
    return CompareToHelper(*this, other);
  } else {
    return -CompareToHelper(other, *this);
  }
}

// Note that bits > size end up in undefined states when sizes
// aren't equal, but that's okay.
template <typename W>
void BasicBitmap<W>::Union(const BasicBitmap<W>& other) {
  const size_t this_array_size = array_size();
  const size_t other_array_size = other.array_size();
  const size_t min_array_size = std::min(this_array_size, other_array_size);
  if (min_array_size == 0) {
    // Nothing to do.
    return;
  }
  // Perform bitwise OR of all but the last common word.
  const size_t last = min_array_size - 1;
  std::transform(map_, map_ + last, other.map_, map_,
                 [](Word a, Word b) { return a | b; });
  // Perform bitwise OR of the last common word, applying mask if necessary.
  map_[last] |= other_array_size == min_array_size
                    ? other.map_[last] & other.HighOrderMapElementMask()
                    : other.map_[last];
}

// Note that bits > size end up in undefined states when sizes
// aren't equal, but that's okay.
template <typename W>
void BasicBitmap<W>::Intersection(const BasicBitmap<W>& other) {
  const size_t this_array_size = array_size();
  const size_t other_array_size = other.array_size();
  const size_t min_array_size = std::min(this_array_size, other_array_size);
  // Perform bitwise AND of all common words.
  std::transform(map_, map_ + min_array_size, other.map_, map_,
                 [](Word a, Word b) { return a & b; });
  if (other_array_size == min_array_size) {
    // Zero out bits that are outside the range of 'other'.
    if (other_array_size != 0) {
      map_[other_array_size - 1] &= other.HighOrderMapElementMask();
    }
    std::fill(map_ + other_array_size, map_ + this_array_size, 0);
  }
}

template <typename W>
bool BasicBitmap<W>::IsIntersectionNonEmpty(const BasicBitmap<W>& other) const {
  // First check fully overlapping bytes.
  size_t max_overlap = std::min(array_size(), other.array_size()) - 1;
  for (size_t i = 0; i < max_overlap; ++i) {
    if (map_[i] & other.map_[i]) return true;
  }

  // Now check the highest overlapping byte, applying bit masks as necessary.
  Word high_byte = map_[max_overlap] & other.map_[max_overlap];

  if (other.array_size() > array_size())
    return high_byte & HighOrderMapElementMask();
  else if (array_size() > other.array_size())
    return high_byte & other.HighOrderMapElementMask();

  // Same array_size, apply both masks.
  return high_byte & HighOrderMapElementMask() &
         other.HighOrderMapElementMask();
}

/*static*/
template <typename W>
void BasicBitmap<W>::SetRange(size_type begin, size_type end, bool value) {
  if (begin == end) return;
  // Figure out which element(s) in the map_ array are affected
  // by this op.
  const size_type begin_element = begin / kIntBits;
  const size_type begin_bit = begin % kIntBits;
  const size_type end_element = end / kIntBits;
  const size_type end_bit = end % kIntBits;
  Word initial_mask = ~W{0} << begin_bit;
  if (end_element == begin_element) {
    // The range is contained in a single element of the array, so
    // adjust both ends of the mask.
    initial_mask = initial_mask & (~W{0} >> (kIntBits - end_bit));
  }
  if (value) {
    map_[begin_element] |= initial_mask;
  } else {
    map_[begin_element] &= ~initial_mask;
  }
  if (end_element != begin_element) {
    // Set all the bits in the array elements between the begin
    // and end elements.
    std::fill(map_ + begin_element + 1, map_ + end_element,
              value ? ~W{0} : W{0});

    // Update the appropriate bit-range in the last element.
    // Note end_bit is an exclusive bound, so if it's 0 none of the
    // bits in end_element are contained in the range (and we don't
    // have to modify it).
    if (end_bit != 0) {
      const Word final_mask = ~W{0} >> (kIntBits - end_bit);
      if (value) {
        map_[end_element] |= final_mask;
      } else {
        map_[end_element] &= ~final_mask;
      }
    }
  }
}
}  // namespace internal
}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_BITMAP_BITMAP_H_
