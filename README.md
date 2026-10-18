# webgpu

A C++20 project built with CMake and Ninja.

## Requirements

- CMake 3.25 or newer (needed for `CMakePresets.json`)
- Ninja
- A C++20 compiler (Clang, GCC or MSVC)

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

## Run

```bash
./build/debug/webgpu
```

Use `./build/release/webgpu` for the release build.
