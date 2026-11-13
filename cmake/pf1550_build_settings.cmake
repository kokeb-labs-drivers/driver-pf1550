#===============================================================================
# PF1550 driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(PF1550_TARGET_NAME "pf1550")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(PF1550_VERSION_MAJOR 0)
set(PF1550_VERSION_MINOR 1)
set(PF1550_VERSION_PATCH 0)
set(PF1550_VERSION "${PF1550_VERSION_MAJOR}.${PF1550_VERSION_MINOR}.${PF1550_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(PF1550_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/pf1550_version.h.in")
set(PF1550_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/pf1550_generated")
set(PF1550_VERSION_HEADER "${PF1550_VERSION_HEADER_DIR}/pf1550_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${PF1550_VERSION_HEADER_DIR}")
configure_file("${PF1550_VERSION_TEMPLATE}" "${PF1550_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(PF1550_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${PF1550_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/pf1550.ipp, included by inc/pf1550.hpp.
set(PF1550_SOURCE_FILES)

set(PF1550_IDF_REQUIRES)
