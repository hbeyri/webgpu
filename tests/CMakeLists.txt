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

add_test(NAME webgpuapp_tests COMMAND webgpuapp_tests)
