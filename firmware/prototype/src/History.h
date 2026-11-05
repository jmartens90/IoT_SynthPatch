// History.h - the last N samples in a fixed block of memory (a ring buffer).
//
// C++ lesson: CLASS TEMPLATE
// --------------------------
// `template <typename T, size_t N>` means: the compiler writes a separate version of this class
// for every T and N you use. History<Sample, 600> stores 600 Samples in a std::array (no heap
// allocation, the size is known at compile time). When it's full, the oldest sample is
// overwritten. at(0) is the oldest, at(size()-1) the newest.
#pragma once

#include <array>
#include <cstddef>
#include <mutex>

template <typename T, size_t N>
class History {
 public:
  void push(const T &v) {
    std::lock_guard<std::mutex> lock(m_);
    data_[head_] = v;
    head_ = (head_ + 1) % N;
    if (count_ < N) count_++;
  }

  size_t size() const { return count_; }
  static constexpr size_t capacity() { return N; }

  // Call fn(sample) for every stored sample, oldest first, under the lock. A function
  // template parameter accepts any lambda.
  template <typename Fn>
  void forEach(Fn fn) {
    std::lock_guard<std::mutex> lock(m_);
    size_t start = (head_ + N - count_) % N;
    for (size_t i = 0; i < count_; i++) fn(data_[(start + i) % N]);
  }

 private:
  std::array<T, N> data_{};
  size_t head_ = 0;
  size_t count_ = 0;
  std::mutex m_;
};
