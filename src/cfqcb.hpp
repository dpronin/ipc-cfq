#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>

#include "align.hpp"
#include "mapping.hpp"

namespace cfq {

template <std::integral T1, std::integral T2> struct cfqcb {
  template <typename T> using pos_t = cfq::uptrwd<T>;
  pos_t<T1> head;
  pos_t<T2> tail;
};

template <typename CbT> auto make_cfqcb(std::shared_ptr<CbT> p_cb) {
  using T1 = decltype(p_cb->head);
  using T2 = decltype(p_cb->tail);
  static_assert(std::is_same_v<std::decay_t<T1>, std::decay_t<T2>>);

  if (!p_cb)
    return cfqcb<T1, T2>{};
  return cfqcb<T1, T2>{
      .head = {&p_cb->head, [p_cb](auto *p) {}},
      .tail = {&p_cb->tail, [p_cb](auto *p) {}},
  };
}

} // namespace cfq
