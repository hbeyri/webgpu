# webgpu

[![CI](https://github.com/hbeyri/webgpu/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/hbeyri/webgpu/actions/workflows/ci.yml)

A C++20 project built with CMake and Ninja.

**Live web build:** <https://hbeyri.github.io/webgpu/> (needs a browser with WebGPU). CI publishes it from `main` after every push that builds. To build it yourself, see [Web build](#web-build).

## Requirements

- CMake 3.25 or newer (needed for `CMakePresets.json`)
- Ninja
- A C++20 compiler (Clang, GCC or MSVC)
- Git, Python 3 and an internet connection the first time you configure (to download and build Dawn, see below)
- [Emscripten](https://emscripten.org) for the web build only (see [Web build](#web-build))

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

## Test

The unit tests use [doctest](https://github.com/doctest/doctest) and are written in the source files themselves, next to the code they test, not in separate test files. See [`src/core/Result.cpp`](src/core/Result.cpp) for an example. Run the tests with CTest after building:

```bash
ctest --preset debug
```

Use `ctest --preset release` for the release build. CI runs the tests for both presets.

To add a test, write a `TEST_CASE` below the code in a `.cpp` file in `src/`, inside an `#ifndef DOCTEST_CONFIG_DISABLE` block:

```cpp
int add(int a, int b)
{
	return a + b;
}

#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

TEST_CASE("add")
{
	CHECK(add(1, 1) == 2);   // on failure: reports it and continues
	REQUIRE(add(1, 1) == 2); // on failure: reports it and stops this test case
}

#endif // DOCTEST_CONFIG_DISABLE
```

Each `TEST_CASE` registers itself, so there is no list of tests to maintain.

How it works:

- The source files listed in `WEBGPUAPP_SOURCES` in [`CMakeLists.txt`](CMakeLists.txt) are compiled twice: into `webgpuapp`, and into the test executable `webgpuapp_tests`. Add new source files to that list so their tests are built.
- `webgpuapp` is compiled with `DOCTEST_CONFIG_DISABLE`, which makes `TEST_CASE` and the assertion macros compile to nothing. No test code ends up in the app or in the web build.
- [`tests/main.cpp`](tests/main.cpp) holds doctest's implementation and the `main()` of `webgpuapp_tests`. That is all the [`tests/`](tests) folder is for.
- doctest is a single header, kept in [`doctest/`](doctest) (version 2.5.3).

Things to know:

- `src/main.cpp` can't hold tests, because the test executable has its own `main()`. Put code you want to test in another file.
- The `#ifndef DOCTEST_CONFIG_DISABLE` block is needed because the app still compiles the body of a `TEST_CASE`, with the assertions removed. Without the block, a variable or helper function that only the assertions use is unused in the app, which is a compile error there.
- The tests for a template in a header go in a `.cpp` file next to it, like `src/core/Result.cpp` for `src/core/Result.h`.
- The tests are only built for the native presets, not for the web build.

Running the test executable directly gives doctest's own output and options, for example to run only some test cases:

```bash
./build/debug/tests/webgpuapp_tests --test-case="*some name*"
```

## Web build

The same code also builds to WebAssembly with [Emscripten](https://emscripten.org), to run in a browser. Install Emscripten first (for example `brew install emscripten`, or with [emsdk](https://emscripten.org/docs/getting_started/downloads.html)).

Configure through `emcmake`, which tells CMake to use the Emscripten compiler, then build as usual:

```bash
emcmake cmake --preset web-debug
cmake --build --preset web-debug
```

| Preset        | Build type | Output folder        |
|---------------|------------|----------------------|
| `web-debug`   | Debug      | `build/web-debug`    |
| `web-release` | Release    | `build/web-release`  |

The build produces `webgpuapp.html`, `webgpuapp.js` and `webgpuapp.wasm`. A browser can't load them from a `file://` path, so serve them over HTTP. `emrun` comes with Emscripten, starts a local server and opens the page:

```bash
emrun build/web-debug/webgpuapp.html
```

The program's output shows up in the text box on the page and in the browser console. The browser must support WebGPU (Chrome, Edge, and recent versions of Safari and Firefox).

What is different on the web:

- The browser provides WebGPU, so Dawn is not downloaded or built. The backend is [emdawnwebgpu](https://dawn.googlesource.com/dawn/+/refs/heads/main/src/emdawnwebgpu/pkg/README.md) instead, Dawn's bindings from `webgpu.h` to the browser's JavaScript API. `WEBGPU_BACKEND` defaults to `EMDAWNWEBGPU` when configuring with `emcmake`.
- emdawnwebgpu is downloaded from a [Dawn release](https://github.com/google/dawn/releases) into the build folder, a few hundred kilobytes. `EMDAWNWEBGPU_VERSION` in `CMakeLists.txt` picks the release. It is set to `v20260914.212600`, the release closest to where the `chromium/8059` branch was cut, so the API matches the native build.
- GLFW comes from Emscripten (`-sUSE_GLFW=3`) instead of the [`glfw/`](glfw) folder.
- The program is linked with `-sASYNCIFY`. Browsers finish WebGPU requests asynchronously, and this lets `wgpuInstanceWaitAny` wait for them, which `requestAdapterSync` and `requestDeviceSync` rely on.
- Code under `#ifdef WEBGPU_BACKEND_DAWN` (the Dawn toggles) is left out. Use `WEBGPU_BACKEND_EMDAWNWEBGPU` or `__EMSCRIPTEN__` for web-only code.

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
| `EMDAWNWEBGPU_VERSION`     | a Dawn release tag       | `v20260914.212600` | Which emdawnwebgpu release the [web build](#web-build) uses |

`WGPU` uses [wgpu-native](https://github.com/gfx-rs/wgpu-native), Firefox's implementation.

Prebuilt Dawn only exists for some versions. To use it, set both options, for example `-DWEBGPU_BUILD_FROM_SOURCE=OFF -DDAWN_VERSION=7187`. The prebuilt binaries come from [eliemichel/dawn-prebuilt](https://github.com/eliemichel/dawn-prebuilt), and Dawn is then a shared library copied next to `webgpuapp` (`libwebgpu_dawn.dylib` on macOS). Note that the code in `src/` is written for the API of Dawn 8059. Other versions may need code changes, for example 7187 uses `capabilities.timedWaitAnyEnable` instead of `requiredFeatures` in `WGPUInstanceDescriptor`.

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

5. **Build and fix API changes.** Dawn's API changes between versions, so the code in `src/` may need updates. The header your code is compiled against is `build/<preset>/_deps/dawn-build/gen/include/dawn/webgpu.h`.

6. **Repeat step 4 for the other presets** (for example `release`). They reuse the source downloaded in step 4.

7. **Update the web build.** Set `EMDAWNWEBGPU_VERSION` in `CMakeLists.txt` to the [Dawn release](https://github.com/google/dawn/releases) closest to where the new branch was cut. Release tags are dates (`v<year><month><day>.<time>`). The branch's [commit log](https://dawn.googlesource.com/dawn/+log/refs/heads/chromium/8059) shows when it was cut: it is the newest commit whose title doesn't start with a milestone tag like `[M155]`. Then configure with `emcmake cmake --preset web-debug -DEMDAWNWEBGPU_VERSION=<tag>` and build.

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
- A function that can fail returns an `HB::Result<T, E>` instead of throwing, a C++ version of Rust's `Result`. See [`src/core/Result.h`](src/core/Result.h).

The [test executable](#test) is the exception: for it, the files in `src/` are compiled a second time with RTTI and exceptions enabled, because doctest uses an exception to stop a test case when a `REQUIRE` fails.
