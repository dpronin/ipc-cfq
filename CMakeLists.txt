cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(ipc-cfq VERSION 1.0.0 LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_CXX_STANDARD_REQUIRED on)

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

add_executable(${PROJECT_NAME}
    src/align.hpp
    src/cb.hpp
    src/cellc.hpp
    src/celld.hpp
    src/cfq.hpp
    src/cfqcb.hpp
    src/cmd.hpp
    src/concepts.hpp
    src/consumer.cpp
    src/consumer.hpp
    src/file.hpp
    src/main.cpp
    src/mapping.hpp
    src/mem.hpp
    src/producer.cpp
    src/producer.hpp
    src/qcmd.hpp
)

if (NOT ${CMAKE_CXX_COMPILER} MATCHES ".*clang.*")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-interference-size)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    fmt::fmt
    spdlog::spdlog
)

