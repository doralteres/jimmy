# Building Jimmy

## Prerequisites

### macOS
- **Xcode** (with command-line tools): `xcode-select --install`
- **CMake** 3.22+: `brew install cmake`

### Windows
- **Visual Studio 2022** (with "Desktop development with C++" workload)
- **CMake** 3.22+: download from https://cmake.org/download/ or `winget install Kitware.CMake`

## Clone & Initialize

```bash
git clone <repo-url> jimmy
cd jimmy
git submodule update --init --recursive
```

## Build

### macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(sysctl -n hw.ncpu)
```

The VST3 bundle will be at:
```
build/Jimmy_artefacts/Release/VST3/Jimmy.vst3
```

### Windows

```powershell
cmake -B build
cmake --build build --config Release
```

The VST3 bundle will be at:
```
build\Jimmy_artefacts\Release\VST3\Jimmy.vst3
```

## Install

See [INSTALL.md](INSTALL.md) for instructions on installing the plugin into Cubase.

## Unit Tests

The project uses Google Test for unit testing. Tests are opt-in via `JIMMY_BUILD_TESTS`.

### macOS

```bash
cmake -B build -DJIMMY_BUILD_TESTS=ON
cmake --build build --target JimmyTests -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

### Windows

```powershell
cmake -B build -DJIMMY_BUILD_TESTS=ON
cmake --build build --target JimmyTests --config Release
ctest --test-dir build --output-on-failure -C Release
```

Google Test is fetched automatically via CMake `FetchContent` — no extra dependencies needed.
