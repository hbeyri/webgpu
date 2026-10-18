cmake_minimum_required(VERSION 3.24)

project(webgpuapp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use Dawn as the default WebGPU backend; override with -DWEBGPU_BACKEND=WGPU
set(WEBGPU_BACKEND "DAWN" CACHE STRING "Backend implementation of WebGPU. Possible values are EMSCRIPTEN, EMDAWNWEBGPU, WGPU and DAWN")

# Include webgpu directory, to define the 'webgpu' target
add_subdirectory(webgpu)

add_executable(${PROJECT_NAME} main.cpp)

# Treat warnings as errors for our own code only, not for dependencies like Dawn
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_WARNING_AS_ERROR ON)

# Add the 'webgpu' target as a dependency of our App
target_link_libraries(${PROJECT_NAME} PRIVATE webgpu)

# The application's binary must find wgpu.dll or libwgpu.so at runtime,
# so we automatically copy it (it's called WGPU_RUNTIME_LIB in general)
# next to the binary.
target_copy_webgpu_binaries(${PROJECT_NAME})

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

