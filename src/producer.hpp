#pragma once

#include <cstdint>

#include <filesystem>
#include <span>

#include "cellc.hpp"
#include "celld.hpp"
#include "cmd.hpp"
#include "qcmd.hpp"

namespace cfq {

struct producer_cfq {
  uint16_t bsize;
};

int producer(qcmd_t<cmd, uint32_t, uint32_t> &qcmd, std::span<celld> cellds,
             cellc &cellc, std::filesystem::path const &p,
             producer_cfq const &cfg);

} // namespace cfq
