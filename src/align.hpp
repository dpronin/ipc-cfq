#pragma once

#ifndef __cpp_lib_hardware_interference_size

#include <cstddef>

#include "boost/lockfree/detail/prefix.hpp"
#endif

namespace cfq {

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
constexpr std::size_t hardware_constructive_interference_size =
    BOOST_LOCKFREE_CACHELINE_BYTES;
constexpr std::size_t hardware_destructive_interference_size =
    BOOST_LOCKFREE_CACHELINE_BYTES;
#endif

} // namespace cfq
