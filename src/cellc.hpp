#pragma once

#include <cstddef>
#include <cstdint>

#include <semaphore.h>

#include "align.hpp"

namespace cfq {

struct cellc {
  sem_t cell_vacant;
  uint16_t cell_sz;
  uint16_t cells_len;
  alignas(hardware_destructive_interference_size) std::byte cells[];
};

} // namespace cfq
