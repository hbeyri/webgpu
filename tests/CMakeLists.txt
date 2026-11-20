# The test executable. The tests themselves are written in the source files in
# src/, which are compiled again here with the tests enabled.
add_executable(webgpuapp_tests
    main.cpp
    ${WEBGPUAPP_SOURCES}
)
target_link_libraries(webgpuapp_tests PRIVATE webgpu glfw glfw3webgpu doctest)
webgpuapp_set_warnings(webgpuapp_tests)
if(NOT webgpu_is_static_dawn)
    target_copy_webgpu_binaries(webgpuapp_tests)
endif()

# doctest's REQUIRE needs exceptions, which the top-level CMakeLists.txt
# removes from MSVC's default flags
if(MSVC)
    target_compile_options(webgpuapp_tests PRIVATE /EHsc)
endif()

# Register each TEST_CASE as its own CTest test, so CTest and the VS Code
# Testing view list and run them one by one. After each build, the test
# executable is run with --list-test-cases to find them, so adding a TEST_CASE
# doesn't need CMake to be re-run.
include("${PROJECT_SOURCE_DIR}/doctest/cmake/doctest.cmake")
doctest_discover_tests(webgpuapp_tests)
