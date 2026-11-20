# doctest v2.5.3, a single-header C++ testing framework.
#   https://github.com/doctest/doctest
# This folder only holds the header (doctest/doctest.h), the license, and the
# CMake scripts for doctest_discover_tests (cmake/, from scripts/cmake/ in the
# doctest repository). To update, replace them with the files from a newer
# release tag.

# Defines the 'doctest' target. SYSTEM keeps our warning flags from applying to
# the doctest header.
add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
