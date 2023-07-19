#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <semaphore.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <new>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/constants.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include <fmt/format.h>

#include <spdlog/spdlog.h>

#include "cb.hpp"
#include "cellc.hpp"
#include "celld.hpp"
#include "cfqcb.hpp"
#include "cmd.hpp"
#include "consumer.hpp"
#include "mapping.hpp"
#include "producer.hpp"
#include "qcmd.hpp"

constexpr uint16_t kCmdsMax = 5;
constexpr uint16_t kCellSize = 512;
constexpr uint16_t kCellsNum = 8;

using pcmds_t = cfq::uptrwd<cfq::cmd>;
using pcelld_t = cfq::uptrwd<cfq::celld>;
using pcellc_t = cfq::uptrwd<cfq::cellc>;
using pcellcs_t = std::shared_ptr<cfq::cellc>;

namespace {

void show_help(std::string_view program) {
  std::cout << fmt::format("{} <path/from>:<path/to> <path/from>:<path/to> ...",
                           program)
            << std::endl;
}

pcmds_t make_cmds(uint16_t cmds_len) {
  auto p_cmds = cfq::map_shared<cfq::cmd>(sizeof(cfq::cmd) * cmds_len);
  if (!p_cmds)
    return {};

  auto *p_cmdsr = p_cmds.get();
  std::memset(p_cmdsr, 0, sizeof(p_cmdsr[0]) * cmds_len);

  return p_cmds;
}

cfq::uptrwd<cfq::cb<uint32_t>> make_qcb() {
  using cb = cfq::cb<uint32_t>;
  return cfq::map_shared<cb>(sizeof(cb));
}

pcelld_t make_cellds(uint16_t cells_len) {
  auto p_cellds = cfq::map_shared<cfq::celld>(sizeof(cfq::celld) * cells_len);
  if (!p_cellds)
    return {};

  std::memset(p_cellds.get(), 0, sizeof(cfq::celld) * cells_len);

  return p_cellds;
}

pcellc_t make_cellc(uint16_t cell_sz, uint16_t cells_len) {
  auto p_cellc =
      cfq::map_shared<cfq::cellc>(sizeof(cfq::cellc) + cell_sz * cells_len);
  if (!p_cellc)
    return {};

  if (sem_init(&p_cellc->cell_vacant, 1, cells_len) < 0)
    return {};

  auto cellc_d = std::move(p_cellc.get_deleter());
  auto *p_cellcr = p_cellc.release();

  try {
    p_cellc = pcellc_t(p_cellcr, [cellc_d, pid = getpid()](auto *p_cellcr) {
      if (pid == getpid())
        sem_destroy(&p_cellcr->cell_vacant);
      cellc_d(p_cellcr);
    });
  } catch (...) {
    sem_destroy(&p_cellcr->cell_vacant);
    cellc_d(p_cellcr);
    throw;
  }

  p_cellc->cell_sz = cell_sz;
  p_cellc->cells_len = cells_len;
  std::memset(p_cellc->cells, 0, p_cellc->cell_sz * p_cellc->cells_len);

  return p_cellc;
}

} // namespace

/* Run example: ./cfq <path/from>:<path/to> <path/from>:<path/to> ... */
int main(int argc, char const *argv[]) {
  int r = EXIT_SUCCESS;

  if (argc < 2) {
    spdlog::critical("there must be at least 2 parameters");
    show_help(argv[0]);
    return EXIT_FAILURE;
  }

  enum : uint32_t {
    kRoleReader,
    kRoleWriter,
    kRolesQty,
  };

  std::vector<std::array<std::filesystem::path, kRolesQty>> path_pairs;
  try {
    std::ranges::transform(
        std::span{argv + 1, static_cast<size_t>(argc) - 1},
        std::back_inserter(path_pairs), [](auto const *arg) {
          std::vector<std::filesystem::path> paths;
          boost::split(paths, arg, boost::is_any_of(":"),
                       boost::token_compress_on);
          if (2 != paths.size() ||
              std::ranges::any_of(
                  paths, [](auto const &path) { return path.empty(); })) {
            throw std::invalid_argument(
                fmt::format("invalid format of input parameter '{}'", arg));
          }
          return std::array{std::move(paths[kRoleReader]),
                            std::move(paths[kRoleWriter])};
        });
  } catch (std::exception const &ex) {
    spdlog::critical(ex.what());
    show_help(argv[0]);
    return EXIT_FAILURE;
  }

  std::vector<std::pair<pid_t, std::shared_ptr<cfq::cellc>>> children;

  /*
   * Create up to path_pairs.size() children by forking
   */
  for (auto const &path_pair : path_pairs) {
    auto p_cmds = make_cmds(kCmdsMax + 1);
    if (!p_cmds) {
      r = EXIT_FAILURE;
      break;
    }

    auto p_qcb = make_qcb();
    if (!p_qcb) {
      r = EXIT_FAILURE;
      break;
    }

    auto p_qcmd = cfq::make_qcmd(
        cfq::make_cfqcb(std::shared_ptr<cfq::cb<uint32_t>>{std::move(p_qcb)}),
        std::span{p_cmds.get(), kCmdsMax + 1});
    if (!p_qcmd) {
      r = EXIT_FAILURE;
      break;
    }

    auto p_cellc = pcellcs_t{make_cellc(kCellSize, kCellsNum)};
    if (!p_cellc) {
      r = EXIT_FAILURE;
      break;
    }

    auto p_cellds = make_cellds(p_cellc->cells_len);
    if (!p_cellds) {
      r = EXIT_FAILURE;
      break;
    }

    cfq::producer_cfq const pr_cfg{
        static_cast<uint16_t>(p_cellc->cells_len * p_cellc->cell_sz)};

    std::array<std::function<int()>, kRolesQty> child_handlers{
        [&] {
          return producer(*p_qcmd, {p_cellds.get(), p_cellc->cells_len},
                          *p_cellc, path_pair[kRoleReader], pr_cfg);
        },
        [&] {
          return consumer(*p_qcmd, {p_cellds.get(), p_cellc->cells_len},
                          *p_cellc, path_pair[kRoleWriter]);
        },
    };

    for (uint32_t role_id = 0; role_id < kRolesQty; ++role_id) {
      if (auto const child_pid = fork(); 0 == child_pid) {
        children.clear();
        children.shrink_to_fit();
        auto handler = std::move(child_handlers[role_id]);
        child_handlers = {};
        return handler ? handler() : EXIT_FAILURE;
      } else if (child_pid > 0) {
        /* We're in a parent's body, remember a new child's PID */
        children.push_back({child_pid, p_cellc});
      } else {
        spdlog::error("{}: failed to create child, reason: {}", getpid(),
                      strerror(errno));
        break;
      }
    }

    if (0 != (children.size() % 2)) {
      if (kill(std::get<0>(children.back()), SIGTERM) < 0)
        kill(std::get<0>(children.back()), SIGKILL);
      children.pop_back();
    }
  }

  /* While there are children we would wait for them all to exit */
  while (!children.empty()) {
    int wstatus{0};
    auto const child = waitpid(-1, &wstatus, 0);
    if (WIFEXITED(wstatus))
      spdlog::info("child {} exited", child);
    std::erase_if(children, [child](auto const &desc) {
      return std::get<0>(desc) == child;
    });
  }

  return r;
}
