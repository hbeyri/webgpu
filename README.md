# webgpu

A C++20 project built with CMake and Ninja.

## Requirements

- CMake 3.25 or newer (needed for `CMakePresets.json`)
- Ninja
- A C++20 compiler (Clang, GCC or MSVC)
- Git, Python 3 and an internet connection the first time you configure (to download and build Dawn, see below)

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

- The default backend is [Dawn](https://dawn.googlesource.com/dawn), Chrome's implementation, built from source from the `chromium/8059` branch (the one used by Chrome 155 stable). `CMakeLists.txt` sets these defaults.
- The first configure downloads the Dawn source and its dependencies once into `build/_shared/dawn-<version>`, shared by all presets. Each preset still compiles Dawn in its own build folder. Later configures and builds reuse both. To keep the shared source somewhere else, set `DAWN_SHARED_SOURCE_ROOT` (the folder that holds the `dawn-<version>` subfolders).
- Before Dawn fetches its dependencies, [`cmake/patches/dawn-8059.patch`](cmake/patches/dawn-8059.patch) is applied to its `tools/fetch_dawn_dependencies.py`. It skips the dependencies this project doesn't build: the DirectX shader compiler and headers, protobuf, googletest and google benchmark. Since protobuf is skipped, `CMakeLists.txt` also sets `DAWN_BUILD_PROTOBUF=OFF`. If you change `DAWN_VERSION`, add a patch for that version (`cmake/patches/dawn-<version>.patch`), or Dawn fetches all its dependencies.
- Dawn is linked statically, so `webgpuapp` doesn't need any extra library file next to it.

### Options

Set these when configuring, for example:

```bash
cmake --preset debug -DWEBGPU_BACKEND=WGPU
```

| Option                     | Values                   | Default  | Meaning                                                    |
|----------------------------|--------------------------|----------|------------------------------------------------------------|
| `WEBGPU_BACKEND`           | `DAWN`, `WGPU`           | `DAWN`   | Which WebGPU implementation to use                         |
| `WEBGPU_BUILD_FROM_SOURCE` | `ON`, `OFF`              | `ON`     | Build Dawn from source (`ON`) or download a prebuilt one (`OFF`) |
| `DAWN_VERSION`             | a Dawn branch number     | `8059`   | Which `chromium/<number>` branch of Dawn to use            |

`WGPU` uses [wgpu-native](https://github.com/gfx-rs/wgpu-native), Firefox's implementation.

Prebuilt Dawn only exists for some versions. To use it, set both options, for example `-DWEBGPU_BUILD_FROM_SOURCE=OFF -DDAWN_VERSION=7187`. The prebuilt binaries come from [eliemichel/dawn-prebuilt](https://github.com/eliemichel/dawn-prebuilt), and Dawn is then a shared library copied next to `webgpuapp` (`libwebgpu_dawn.dylib` on macOS). Note that `main.cpp` is written for the API of Dawn 8059. Other versions may need code changes, for example 7187 uses `capabilities.timedWaitAnyEnable` instead of `requiredFeatures` in `WGPUInstanceDescriptor`.

Options are saved in the build folder's cache. To switch an existing build folder back to the defaults, configure it with `cmake --preset debug --fresh`.

### Updating Dawn

Dawn has no numbered releases. Each Chrome version is built from a Dawn branch named `chromium/<number>`, so updating Dawn means picking a newer branch number.

1. **Find the branch number.** Look up the current Chrome stable version on [chromiumdash](https://chromiumdash.appspot.com/releases?platform=Mac). The third part of the version is the branch number, for example `155.0.8059.85` is branch `8059`. Check that Dawn has that branch:

   ```bash
   git ls-remote https://dawn.googlesource.com/dawn refs/heads/chromium/8059
   ```

   It prints a commit hash if the branch exists, and nothing if it doesn't.

2. **Change the default version.** In `CMakeLists.txt`, set `DAWN_VERSION` to the new number (and update the Chrome version in the comment above it).

3. **Add a patch for the new version.** Start from the current one:

   ```bash
   cp cmake/patches/dawn-8059.patch cmake/patches/dawn-<number>.patch
   ```

4. **Configure.** `DAWN_VERSION` is saved in each build folder's cache, so pass the new number once (or use `--fresh`):

   ```bash
   cmake --preset debug -DDAWN_VERSION=<number>
   ```

   The new source is downloaded into `build/_shared/dawn-<number>`. If the copied patch still fits, it's applied and configuring continues. If it doesn't, configuring stops with "Applying ... to Dawn failed" before Dawn fetches its dependencies. In that case, make the same change by hand in `build/_shared/dawn-<number>/tools/fetch_dawn_dependencies.py`: move the DirectX shader compiler, DirectX headers, libprotobuf-mutator, protobuf, google benchmark and googletest entries from the default list into the `if args.use_test_deps:` list. Then save the change as the new patch and configure again:

   ```bash
   git -C build/_shared/dawn-<number> diff tools/fetch_dawn_dependencies.py > cmake/patches/dawn-<number>.patch
   cmake --preset debug -DDAWN_VERSION=<number>
   ```

5. **Build and fix API changes.** Dawn's API changes between versions, so `main.cpp` may need updates. The header your code is compiled against is `build/<preset>/_deps/dawn-build/gen/include/dawn/webgpu.h`.

6. **Repeat step 4 for the other presets** (for example `release`). They reuse the source downloaded in step 4.

Once everything builds, you can delete the old `build/_shared/dawn-<old number>` folder and the old patch.

The C++ wrapper `webgpu.hpp` in `webgpu/dawn/include` is generated for Dawn 7187, the version the `webgpu/` distribution was made for. If you start using it, regenerate it for your Dawn version with [WebGPU-Cpp](https://eliemichel.github.io/WebGPU-Cpp).

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
