#pragma once

#include <climits>
#include <cstdint>

#include <sstream>
#include <sys/types.h>

#include <limits>
#include <ostream>
#include <string>

#include <fmt/format.h>

namespace cfq {

constexpr uint8_t bits_to_mask(uint8_t bits) { return (1 << bits) - 1; }

constexpr uint8_t qty_to_bits(uint8_t qty) {
  return qty <= (1 << 1)   ? 1
         : qty <= (1 << 2) ? 2
         : qty <= (1 << 3) ? 3
         : qty <= (1 << 4) ? 4
         : qty <= (1 << 5) ? 5
         : qty <= (1 << 6) ? 6
         : qty <= (1 << 7) ? 7
                           : 8;
}

enum : uint8_t {
  op_read = 0,
  op_write = 1,
  op_eof = 2,

  ops_qty,
};

constexpr uint8_t op_shift = 0;
constexpr uint8_t op_bits = qty_to_bits(ops_qty);
constexpr uint8_t op_mask = bits_to_mask(op_bits);

constexpr uint8_t fl_shift = op_bits;
constexpr uint8_t fl_mask = ~(op_mask);
constexpr uint8_t fl_bits = CHAR_BIT - op_bits;

struct cmd {
  uint16_t id;
  uint8_t opfl;
  uint8_t reserved1;
  uint16_t fcdn;
  uint16_t cnum;

  uint16_t get_id() const noexcept { return id; }
  void set_id(uint16_t v) noexcept { id = v; }

  uint16_t get_fcdn() const noexcept { return fcdn; }
  void set_fcdn(uint16_t n) noexcept { fcdn = n; }

  uint16_t get_cnum() const noexcept { return cnum; }
  void set_cnum(uint16_t n) noexcept { cnum = n; }

  [[nodiscard]] uint_fast32_t get_op() const noexcept {
    return (opfl & op_mask) >> op_shift;
  }
  void set_op(uint_fast32_t op) noexcept {
    opfl &= ~op_mask;
    opfl |= ((op & op_mask) << op_shift);
  }

  [[nodiscard]] uint_fast32_t get_fl() const noexcept {
    return (opfl & fl_mask) >> fl_shift;
  }
  void set_fl(uint_fast32_t fl) noexcept {
    opfl &= ~fl_mask;
    opfl |= (fl << fl_shift);
  }

  friend std::ostream &operator<<(std::ostream &out, cmd const &cmd) {
    out << fmt::format("cmd: [ id={}, op={}, fl={}, fcdn={}, cnum={} ]",
                       cmd.get_id(), cmd.get_op(), cmd.get_fl(), cmd.get_fcdn(),
                       cmd.get_cnum());
    return out;
  }
};

} // namespace cfq

template <> struct fmt::formatter<cfq::cmd> : fmt::formatter<std::string> {
  auto format(cfq::cmd const &cmd, format_context &ctx) -> decltype(ctx.out()) {
    std::ostringstream oss;
    oss << cmd;
    return format_to(ctx.out(), "{}", oss.str());
  }
};
