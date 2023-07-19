#include "consumer.hpp"

#include <cstddef>
#include <cstdlib>

#include <unistd.h>

#include <fstream>
#include <ios>
#include <limits>
#include <thread>

#include <spdlog/spdlog.h>

namespace cfq {

int consumer(qcmd_t<cmd, uint32_t, uint32_t> &qcmd, std::span<celld> cellds,
             cellc &cellc, std::filesystem::path const &p) {
  spdlog::set_pattern("[consumer %P] [%^%l%$]: %v");

  spdlog::info("started: file to write {}", p.string());

  std::ofstream f{p, std::ios::out | std::ios::binary};

  for (bool eof = false; f && !eof;) {
    spdlog::debug("is working");
    if (auto const v = qcmd.pop()) [[likely]] {
      spdlog::debug("processing {} ", *v);
      switch (auto ncell = v->get_fcdn(); v->get_op()) {
      case op_write: {
        auto cells_left = v->get_cnum();
        for (auto *celld = &cellds[ncell];
             cells_left > 0 && ncell < cellc.cells_len;
             celld = &cellds[ncell], --cells_left) {

          f.write(reinterpret_cast<char const *>(cellc.cells +
                                                 cellc.cell_sz * ncell),
                  celld->data_sz);

          ncell = celld->ncell;

          sem_post(&cellc.cell_vacant);
        }
      } break;
      default:
        eof = true;
        continue;
      }
    } else {
      std::this_thread::yield();
    }
  }

  spdlog::info("finished");

  return EXIT_SUCCESS;
}

} // namespace cfq
