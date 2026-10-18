# webgpu

A C++20 project built with CMake and Ninja.

## Requirements

- CMake 3.25 or newer (needed for `CMakePresets.json`)
- Ninja
- A C++20 compiler (Clang, GCC or MSVC)
- An internet connection the first time you configure (to download WebGPU, see below)

## Build

The project uses CMake presets, which set the Ninja generator and the build folder for you.

Debug build:

```bash
cmake --preset debug
cmake --build --preset debug
```

Release build:

```bash
cmake --preset release
cmake --build --preset release
```

| Preset    | Build type | Output folder    |
|-----------|------------|------------------|
| `debug`   | Debug      | `build/debug`    |
| `release` | Release    | `build/release`  |

You only need to run the configure step (`cmake --preset ...`) once, or again after changing `CMakeLists.txt` or the presets.

## Clean

Remove the compiled files but keep the configuration and the downloaded WebGPU:

```bash
cmake --build --preset debug --target clean
```

Clean and rebuild in one step:

```bash
cmake --build --preset debug --clean-first
```

For the release build, replace `debug` with `release`.

## Run

```bash
./build/debug/webgpuapp
```

Use `./build/release/webgpuapp` for the release build.

## WebGPU dependency

WebGPU comes from the [`webgpu/`](webgpu) folder, a copy of [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution) `v0.3.0-gamma` from the Learn WebGPU for C++ guide. It defines the `webgpu` CMake target that `webgpuapp` links against, and also provides the `webgpu.hpp` C++ wrapper.

- The default backend is [Dawn](https://dawn.googlesource.com/dawn), Chrome's implementation, version `chromium/7187`. `CMakeLists.txt` sets this default.
- The first configure downloads a prebuilt Dawn into `build/<preset>/_deps`. Later configures reuse it. The prebuilt binaries come from [eliemichel/dawn-prebuilt](https://github.com/eliemichel/dawn-prebuilt), the distribution's mirror.
- Dawn is a shared library. The build copies it next to the executable (`libwebgpu_dawn.dylib` on macOS), so keep that file with `webgpuapp` when you move it.

### Options

Set these when configuring, for example:

```bash
cmake --preset debug -DWEBGPU_BACKEND=WGPU
```

| Option                     | Values                   | Default  | Meaning                                                    |
|----------------------------|--------------------------|----------|------------------------------------------------------------|
| `WEBGPU_BACKEND`           | `DAWN`, `WGPU`           | `DAWN`   | Which WebGPU implementation to use                         |
| `WEBGPU_BUILD_FROM_SOURCE` | `ON`, `OFF`              | `OFF`    | Build Dawn from source instead of downloading it (needs Python, and is much slower) |
| `WEBGPU_LINK_TYPE`         | `SHARED`, `STATIC`       | `SHARED` | Link WebGPU dynamically or statically (prebuilt Dawn supports `SHARED` only) |

`WGPU` uses [wgpu-native](https://github.com/gfx-rs/wgpu-native), Firefox's implementation.

Options are saved in the build folder's cache. To switch an existing build folder back to the defaults, configure it with `cmake --preset debug --fresh`.

## RTTI and exceptions

The project is compiled with RTTI and exceptions disabled:

| Compiler  | Flags                                       |
|-----------|---------------------------------------------|
| Clang/GCC | `-fno-rtti -fno-exceptions`                 |
| MSVC      | `/GR- /EHs-c-` and `_HAS_EXCEPTIONS=0`      |

This means:

- `dynamic_cast` and `typeid` on polymorphic types don't compile.
- `try`, `catch` and `throw` don't compile.
- Standard library calls that would normally throw (for example a failed `new` or `std::vector::at` out of range) terminate the program instead.
