#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>

#include "align.hpp"
#include "cfqcb.hpp"
#include "concepts.hpp"

namespace cfq {

template <cfq_suitable T, std::integral I1, std::integral I2>
class alignas(hardware_destructive_interference_size) cfq {
public:
  [[nodiscard]] auto capacity() const noexcept { return items_.size(); }

  explicit cfq(cfqcb<I1, I2> cb, std::span<T> items)
      : cb_(std::move(cb)), items_(items) {
    if (!cb_.head)
      throw std::invalid_argument("cb head given cannot be empty");
    if (!cb_.tail)
      throw std::invalid_argument("cb tail given cannot be empty");
    if (items_.size() < 2)
      throw std::invalid_argument(
          "items given must contain at least 2 elements");
    if (*cb_.head > capacity())
      throw std::invalid_argument("head cannot index an item out of range");
    if (*cb_.tail > capacity())
      throw std::invalid_argument("tail cannot index an item out of range");
  }
  ~cfq() = default;

  cfq(cfq const &) = delete;
  cfq operator=(cfq const &) = delete;

  cfq(cfq &&) = delete;
  cfq &operator=(cfq &&) = delete;

  std::optional<T> pop() noexcept(std::is_nothrow_copy_constructible_v<T>
                                      &&std::is_nothrow_destructible_v<T>) {
    std::optional<T> v;

    auto ph = __atomic_load_n(cb_.head.get(), __ATOMIC_RELAXED);
    do {
      if (auto const pt = __atomic_load_n(cb_.tail.get(), __ATOMIC_ACQUIRE);
          pt == ph) [[unlikely]] {

        v.reset();
        break;
      }
      v = items_[ph];
    } while (!__atomic_compare_exchange_n(cb_.head.get(), &ph,
                                          (ph + 1) % capacity(), true,
                                          __ATOMIC_RELEASE, __ATOMIC_ACQUIRE));

    return v;
  }

  bool push(T const &v) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    auto const pt = *cb_.tail;
    auto const npt = (pt + 1) % capacity();
    if (npt == __atomic_load_n(cb_.head.get(), __ATOMIC_RELAXED)) [[unlikely]]
      return false;
    items_[pt] = v;
    __atomic_store_n(cb_.tail.get(), npt, __ATOMIC_RELEASE);
    return true;
  }

private:
  cfqcb<I1, I2> cb_;
  std::span<T> items_;
};

} // namespace cfq
