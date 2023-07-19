#pragma once

#include <span>
#include <utility>

#include "cfq.hpp"
#include "mem.hpp"

namespace cfq {

template <typename T, typename I1, typename I2> using qcmd_t = cfq<T, I1, I2>;
template <typename T, typename I1, typename I2>
using pqcmd_t = uptrwd<qcmd_t<T, I1, I2>>;

template <typename T, typename I1, typename I2>
pqcmd_t<T, I1, I2> make_qcmd(cfqcb<I1, I2> p_cb, std::span<T> cmds) {
  return make_unique<qcmd_t<T, I1, I2>>(std::move(p_cb), cmds);
}

} // namespace cfq
