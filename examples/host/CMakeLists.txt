# =============================================================================
# PF1550 driver: host example (any PC, no hardware)
# =============================================================================
# The driver against a simulated PF1550 (the register model the host tests
# use, tests/mock_pf1550.hpp) through the host port in this folder.
#
#   cmake -S examples/host -B build-example
#   cmake --build build-example && ./build-example/pf1550_host_example
#
# The host tests also build this folder and run the example (CTest
# pf1550_host_example), so it always compiles against the current headers.
# =============================================================================
cmake_minimum_required(VERSION 3.16)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(pf1550_host_example LANGUAGES CXX)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../.. pf1550)
endif()

# ISO C++20, also when this folder is built on its own (handbook KB-CPP20-001).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pf1550_host_example main.cpp)
target_link_libraries(pf1550_host_example PRIVATE pf1550::pf1550)
target_include_directories(pf1550_host_example PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/../../tests)
target_compile_features(pf1550_host_example PRIVATE cxx_std_20)
target_compile_options(pf1550_host_example PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic
  -pedantic-errors)
