# Building CRUMBS

## On a Mac (the release build)

The GitHub Actions workflow in `.github/workflows/macos.yml` makes a universal (arm64 + x86_64) VST3, runs engine and plugin tests, and runs pluginval at strictness 10. Private-repo Actions have been blocked by billing before; if that is still the case the Mac zip will not appear until it is resolved. The repo stays private.

Locally, with CMake, Xcode and JUCE 8.0.4 in a `JUCE/` folder next to this file:

```
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --parallel
```

The VST3 lands in `build/Crumbs_artefacts/Release/VST3/CRUMBS.vst3`.

## Engine-only (no JUCE)

```
cmake -S . -B build-engine -DCRUMBS_ENGINE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-engine
./build-engine/crumbs_engine_tests
```

## Linux box notes

This project can be configured with Ninja and a C++20 compiler. Point `CMAKE_C_COMPILER` / `CMAKE_CXX_COMPILER` at your compiler, keep a `JUCE/` checkout (8.0.4) next to the project, and build the plugin plus `crumbs_plugin_tests`.
