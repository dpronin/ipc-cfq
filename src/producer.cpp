#include "producer.hpp"

#include <climits>
#include <cstddef>
#include <cstdlib>

#include <semaphore.h>
#include <type_traits>
#include <unistd.h>

#include <concepts>
#include <fstream>
#include <ios>
#include <limits>
#include <thread>

#include <spdlog/spdlog.h>

#include "cmd.hpp"

namespace {

cfq::cmd make_cmd(uint16_t id, uint8_t op, uint16_t fcd, uint16_t cnum) {
  cfq::cmd cmd;

  cmd.set_id(id);
  cmd.set_op(op);
  cmd.set_fl(0);
  cmd.set_fcdn(fcd);
  cmd.set_cnum(cnum);

  return cmd;
}

template <std::integral T> constexpr T div_round_up(T v, T d) noexcept {
  return (v + d - 1) / d;
}

} // namespace

namespace cfq {

int producer(qcmd_t<cmd, uint32_t, uint32_t> &qcmd, std::span<celld> cellds,
             cellc &cellc, std::filesystem::path const &p,
             producer_cfq const &cfg) {
  spdlog::set_pattern("[producer %P] [%^%l%$]: %v");

  spdlog::info("started: file to read {}", p.string());

  uint16_t const max_cells_at_once =
      std::min(div_round_up(cfg.bsize, cellc.cell_sz), cellc.cells_len);

  uint32_t cmd_id = 0;
  uint16_t ncell = 0;

  for (std::ifstream f{p, std::ios::in | std::ios::binary}; f;) {
    spdlog::debug("is working");

    uint16_t cells_len = 0;

    celld dummy_celld{};
    for (celld *prev_celld = &dummy_celld;
         f && cells_len < max_cells_at_once;) {

      if (sem_trywait(&cellc.cell_vacant) < 0) [[unlikely]]
        break;

      uint16_t data_sz{cellc.cell_sz};
      if (!f.read(reinterpret_cast<char *>(cellc.cells + cellc.cell_sz * ncell),
                  data_sz)) [[unlikely]] {

        data_sz = f.gcount();
      }

      if (data_sz) [[likely]] {
        auto *p_celld = &cellds[ncell];
        p_celld->data_sz = data_sz;
        p_celld->ncell = cellc.cells_len;

        prev_celld->ncell = ncell;
        prev_celld = p_celld;

        ncell = (ncell + 1) % cellds.size();
        ++cells_len;
      } else {
        sem_post(&cellc.cell_vacant);
      }
    }

    if (cells_len > 0) [[likely]] {
      auto const v = make_cmd(cmd_id, op_write, dummy_celld.ncell, cells_len);
      while (!qcmd.push(v))
        std::this_thread::yield();

      spdlog::debug("pushed {}", v);

      ++cmd_id;
    } else {
      std::this_thread::yield();
    }
  }

  auto const v = make_cmd(cmd_id, op_eof, 0, 0);

  spdlog::debug("is pushing last cmd {} ...", v);

  while (!qcmd.push(v))
    std::this_thread::yield();

  spdlog::debug("pushed last cmd {} ...", v);

  spdlog::info("finished");

  return EXIT_SUCCESS;
}

} // namespace cfq
