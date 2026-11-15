# WolfVK

WolfVK is a modern Windows reimagining of **Wolfenstein 3D**, written in C. It is being developed around SDL3 for windowing and input, and Vulkan for 3D rendering. It will load the original game's level and graphic data rather than requiring replacement assets.

Development follows the ordered, approval-gated steps in [roadmap.md](roadmap.md). The original [Wolf3D repository](https://github.com/elaverick/Wolf3D) is a read-only reference.

## Current development status

**Current roadmap step:** Ensure the window can render Vulkan 3D content in a performant manner.

The SDL3 window milestone is complete. The application now requests a Vulkan-capable window, creates a Vulkan instance and surface, selects a graphics/presentation device, and creates its logical device. Swapchain creation and actual 3D drawing are still outstanding. Its Windows x64 configure and build passed in [GitHub Actions run #1](https://github.com/elaverick/WolfVK/actions/runs/43708820409).

The test harness uses [Unity](https://github.com/ThrowTheSwitch/Unity), a dedicated C unit-testing framework. The test runner contains sixteen Unity tests: three SDL3 window/event integration tests and six Vulkan queue-family selection unit tests. The Vulkan build dependency is installed explicitly in Windows CI. Tests are registered with CTest and run by the GitHub Actions workflow.

| Check | Current status |
|---|---|
| Windows x64 build | Tracked by the Actions badge below |
| C unit-test framework | Unity |
| Automated tests | 16 Unity tests: 3 SDL3 integration tests and 13 Vulkan queue, surface-format, present-mode and renderer-guard unit tests |
| SDL3 window | Complete |
| Vulkan instance, surface and device selection | Implemented; CI verification pending |
| Original level loading | Not started |
| Playable demo | Not started |

[![Windows build and tests](https://github.com/elaverick/WolfVK/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/elaverick/WolfVK/actions/workflows/build.yml)

The workflow configures and compiles the Windows x64 application, then runs the Unity test suite through CTest. The badge reflects the latest workflow result for the branch.

## Build locally

A C17 compiler, CMake 3.24 or newer, Git, and network access for SDL3 and Unity source dependencies are required.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Controls

There are no gameplay controls yet. Close the application window to exit.
