#pragma once

#include <concepts>

#include "align.hpp"

namespace cfq {

template <std::integral T> struct cb {
  static_assert(sizeof(T) <= hardware_destructive_interference_size);
  alignas(hardware_destructive_interference_size) T head;
  alignas(hardware_destructive_interference_size) T tail;
};

} // namespace cfq
