cmake_minimum_required(VERSION 3.24)

project(webgpuapp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use Dawn as the default WebGPU backend; override with -DWEBGPU_BACKEND=WGPU
set(WEBGPU_BACKEND "DAWN" CACHE STRING "Backend implementation of WebGPU. Possible values are EMSCRIPTEN, EMDAWNWEBGPU, WGPU and DAWN")

# Build Dawn from source, using the branch that matches Chrome stable (Chrome 155)
option(WEBGPU_BUILD_FROM_SOURCE "Build the WebGPU backend from source instead of downloading a prebuilt one" ON)
set(DAWN_VERSION "8059" CACHE STRING "Dawn version to use: the number after 'chromium/' in a Dawn branch name")

# Share one copy of the Dawn source between all build folders (debug, release, ...).
# Each build folder still compiles Dawn on its own, but the source and its
# dependencies are only downloaded once.
set(DAWN_SHARED_SOURCE_DIR "${CMAKE_SOURCE_DIR}/build/_shared/dawn-${DAWN_VERSION}" CACHE PATH
    "Where to keep the shared Dawn source when building Dawn from source")

string(TOUPPER "${WEBGPU_BACKEND}" WEBGPU_BACKEND_UPPER)
# FetchContent saves FETCHCONTENT_SOURCE_DIR_DAWN in the cache once set, so
# only skip this block when it points somewhere else (set by hand on purpose)
if(WEBGPU_BACKEND_UPPER STREQUAL "DAWN" AND WEBGPU_BUILD_FROM_SOURCE
        AND (NOT FETCHCONTENT_SOURCE_DIR_DAWN OR FETCHCONTENT_SOURCE_DIR_DAWN STREQUAL DAWN_SHARED_SOURCE_DIR))
    # The stamp file is written only after a complete download, so an
    # interrupted download is retried on the next configure
    set(dawn_stamp "${DAWN_SHARED_SOURCE_DIR}/.webgpuapp-fetched")
    if(NOT EXISTS "${dawn_stamp}")
        find_package(Git REQUIRED)
        message(STATUS "Downloading Dawn chromium/${DAWN_VERSION} source into '${DAWN_SHARED_SOURCE_DIR}'")
        file(MAKE_DIRECTORY "${DAWN_SHARED_SOURCE_DIR}")
        foreach(git_args
                "init;--quiet"
                "fetch;--depth=1;https://dawn.googlesource.com/dawn;chromium/${DAWN_VERSION}"
                "reset;--hard;--quiet;FETCH_HEAD")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" ${git_args}
                WORKING_DIRECTORY "${DAWN_SHARED_SOURCE_DIR}"
                RESULT_VARIABLE git_result)
            if(NOT git_result EQUAL 0)
                message(FATAL_ERROR "Downloading the Dawn source failed: git ${git_args}")
            endif()
        endforeach()
        file(TOUCH "${dawn_stamp}")
    endif()

    # Apply our patch for this Dawn version, if there is one. It makes Dawn's
    # tools/fetch_dawn_dependencies.py skip dependencies we don't build (DXC,
    # DirectX headers, protobuf, googletest, google benchmark). This has to
    # happen before Dawn's own CMake runs that script.
    set(dawn_patch "${CMAKE_SOURCE_DIR}/cmake/patches/dawn-${DAWN_VERSION}.patch")
    if(EXISTS "${dawn_patch}")
        find_package(Git REQUIRED)
        # If the patch can be reversed, it's already applied
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${dawn_patch}"
            WORKING_DIRECTORY "${DAWN_SHARED_SOURCE_DIR}"
            RESULT_VARIABLE patch_applied
            OUTPUT_QUIET ERROR_QUIET)
        if(NOT patch_applied EQUAL 0)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" apply "${dawn_patch}"
                WORKING_DIRECTORY "${DAWN_SHARED_SOURCE_DIR}"
                RESULT_VARIABLE patch_result
                ERROR_VARIABLE patch_error)
            if(NOT patch_result EQUAL 0)
                message(FATAL_ERROR "Applying '${dawn_patch}' to Dawn failed:\n${patch_error}")
            endif()
            message(STATUS "Applied '${dawn_patch}' to Dawn")
        endif()

        # Dawn's CMakeLists.txt defaults DAWN_BUILD_PROTOBUF to ON, but the patch
        # skips protobuf. A normal variable overrides that option, even if ON is cached.
        set(DAWN_BUILD_PROTOBUF OFF)
    endif()

    # Tell FetchContent (used by webgpu/dawn) to use this folder instead of
    # downloading Dawn into the build folder
    set(FETCHCONTENT_SOURCE_DIR_DAWN "${DAWN_SHARED_SOURCE_DIR}")
endif()

# Include webgpu directory, to define the 'webgpu' target
add_subdirectory(webgpu)

add_executable(${PROJECT_NAME} main.cpp)

# Treat warnings as errors for our own code only, not for dependencies like Dawn
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_WARNING_AS_ERROR ON)

# Add the 'webgpu' target as a dependency of our App
target_link_libraries(${PROJECT_NAME} PRIVATE webgpu)

# When WebGPU is a shared library (.dll/.so/.dylib), the application's binary
# must find it at runtime, so we copy it next to the binary. Dawn built from
# source is a static library linked into the binary, so there's nothing to copy.
set(webgpu_is_static_dawn FALSE)
if(TARGET webgpu_dawn)
    get_target_property(webgpu_dawn_type webgpu_dawn TYPE)
    if(webgpu_dawn_type STREQUAL "STATIC_LIBRARY")
        set(webgpu_is_static_dawn TRUE)
    endif()
endif()
if(NOT webgpu_is_static_dawn)
    target_copy_webgpu_binaries(${PROJECT_NAME})
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
        -Wno-missing-braces -Wno-switch)
endif()

# Disable RTTI and exceptions
if(MSVC)
    # CMake's default CMAKE_CXX_FLAGS has /GR /EHsc; remove them so they don't conflict
    string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(${PROJECT_NAME} PRIVATE /GR- /EHs-c-)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _HAS_EXCEPTIONS=0)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-rtti -fno-exceptions)
endif()

