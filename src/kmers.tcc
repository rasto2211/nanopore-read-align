#include <vector>
#include <string>
#include <queue>
#include <algorithm>

#include "kmers.h"

#include <glog/logging.h>

inline int baseCharToInt(char base) {
  for (int i = 0; i < kNumBases; i++) {
    if (kBases[i] == base) return i;
  }
  LOG(FATAL) << "Found invalid base char: " << base;
  return -1;
}

inline long long numKmersOf(int length) {
  long long res = 1;
  for (int i = 0; i < length; i++) {
    res *= (long long)kNumBases;
  }

  return res;
}

template <typename IntType>
IntType encodeKmer(const std::string& kmer) {
  // Numbers in base @kBases can start with AAA... which is in @kBases 000...
  // Normally, the number of leading zeros doesn't mean anything. In this case
  // we care about it. Therefore we put 1 in front of the number to not lose all
  // the zeros.
  IntType res = (IntType)1;
  for (int idx = 0; idx < (int)kmer.size(); idx++) {
    res = res * kNumBases + (IntType)baseCharToInt(kmer[idx]);
  }
  return res;
}

template <typename IntType>
std::string decodeKmer(IntType code) {
  IntType num = code;
  std::string res;
  while (num > 0) {
    res += kBases[num % kNumBases];
    num /= kNumBases;
  }
  // Get rid of 1 that we artificially added in front of the number. See
  // encodeKmer for description.
  res.pop_back();
  reverse(res.begin(), res.end());

  return res;
}

template <typename IntType>
KmerWindowIterator<IntType>::KmerWindowIterator(
    int k, const std::string::const_iterator& begin_window,
    const std::string::const_iterator& string_end)
    : most_significant_(1),
      first_one_(0),
      begin_window_(begin_window),
      string_end_(string_end) {
  if (string_end_ - begin_window_ < k) {
    end_window_ = string_end;
    current_window_code_ = -1;
    return;
  }

  end_window_ = begin_window_ + k;
  current_window_code_ =
      encodeKmer<IntType>(std::string(begin_window_, end_window_));
  for (int i = 0; i < k - 1; i++) most_significant_ *= kNumBases;
  first_one_ = most_significant_ * kNumBases;
}

template <typename IntType>
IntType KmerWindowIterator<IntType>::next() {
  if (!hasNext()) return (IntType)(-1);

  IntType first_val = baseCharToInt(*begin_window_);
  IntType new_val = baseCharToInt(*end_window_);

  // Remove first char from the window.
  current_window_code_ -= most_significant_ * first_val + first_one_;
  current_window_code_ *= kNumBases;
  // Add new char to the window and add the one in front of the number.
  current_window_code_ += new_val + first_one_;

  begin_window_++;
  end_window_++;

  return current_window_code_;
}
