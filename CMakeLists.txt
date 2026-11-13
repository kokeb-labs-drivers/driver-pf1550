#===============================================================================
# PF1550 driver: root CMake
#===============================================================================
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   pf1550, consumed as pf1550::pf1550 both through add_subdirectory() and
#   through find_package(pf1550) after install.
#
#   cmake -S . -B build -DPF1550_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/pf1550_build_settings.cmake)

#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${PF1550_SOURCE_FILES}
        INCLUDE_DIRS ${PF1550_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${PF1550_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(PF1550_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()

project(pf1550_driver
    VERSION ${PF1550_VERSION}
    DESCRIPTION "C++20 driver for the NXP PF1550 power management IC (single-cell Li-ion linear charger, three buck regulators, three LDOs) on I2C"
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because pf1550.hpp includes
# "../src/pf1550.ipp" relative to itself:
#   <prefix>/include/pf1550/inc/*.hpp, pf1550_version.h
#   <prefix>/include/pf1550/src/pf1550.ipp
set(PF1550_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/pf1550/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${PF1550_TARGET_NAME} INTERFACE)
target_include_directories(${PF1550_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${PF1550_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${PF1550_INSTALL_INCLUDEDIR}>
)
target_compile_features(${PF1550_TARGET_NAME} INTERFACE cxx_std_20)

# pf1550::pf1550 everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(pf1550::pf1550 ALIAS ${PF1550_TARGET_NAME})
set_target_properties(${PF1550_TARGET_NAME} PROPERTIES EXPORT_NAME pf1550)

option(PF1550_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links pf1550::pf1550" OFF)
if(PF1550_ENABLE_WARNINGS)
    target_compile_options(${PF1550_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(PF1550_BUILD_TESTS "Build the host tests in tests/" OFF)
option(PF1550_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(PF1550_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${PF1550_TARGET_NAME} EXPORT pf1550Targets
    INCLUDES DESTINATION ${PF1550_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${PF1550_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${PF1550_VERSION_HEADER}" DESTINATION ${PF1550_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pf1550/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT pf1550Targets
    FILE pf1550Targets.cmake
    NAMESPACE pf1550::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pf1550)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/pf1550Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pf1550Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pf1550)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/pf1550ConfigVersion.cmake"
    VERSION ${PF1550_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/pf1550Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/pf1550ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pf1550)
