# WolfVK

WolfVK is a modern Windows reimagining of **Wolfenstein 3D**, written in C. It is being developed around SDL3 for windowing and input, and Vulkan for 3D rendering. It will load the original game's level and graphic data rather than requiring replacement assets.

Development follows the ordered, approval-gated steps in [roadmap.md](roadmap.md). The original [Wolf3D repository](https://github.com/elaverick/Wolf3D) is a read-only reference.

## Current development status

**Current roadmap step:** Ensure the window can render Vulkan 3D content in a performant manner.

The SDL3 window milestone is complete. A resizable 1280×720 window opens and exits cleanly when closed. Its Windows x64 configure, build and hidden-window smoke test passed in [GitHub Actions run #1](https://github.com/elaverick/WolfVK/actions/runs/43708820409).

| Check | Current status |
|---|---|
| Windows x64 build | Tracked by the Actions badge below |
| Automated tests | SDL3 hidden-window creation smoke test (passed on the completed first milestone) |
| SDL3 window | Complete |
| Vulkan renderer | Current development step |
| Original level loading | Not started |
| Playable demo | Not started |

[![Windows build and tests](https://github.com/elaverick/WolfVK/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/elaverick/WolfVK/actions/workflows/build.yml)

The workflow configures and compiles the Windows x64 application, then runs the SDL3 window-creation smoke test. The badge reflects the latest workflow result for the branch.

## Build locally

A C17 compiler, CMake 3.24 or newer, Git, and network access for SDL3's CMake dependency are required.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Controls

There are no gameplay controls yet. Close the application window to exit.
