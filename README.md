# WolfVK

WolfVK is a modern Windows reimagining of **Wolfenstein 3D**, written in C. It is being developed around SDL3 for windowing and input, and Vulkan for 3D rendering. It will load the original game's level and graphic data rather than requiring replacement assets.

Development follows the ordered, approval-gated steps in [roadmap.md](roadmap.md). The original [Wolf3D repository](https://github.com/elaverick/Wolf3D) is a read-only reference.

## Current development status

**Current roadmap step:** The engine must render quads.

The SDL3 window, Vulkan renderer and input milestones are complete. The application now creates a Vulkan instance and surface, prefers discrete GPUs when selecting a graphics/presentation device, builds a swapchain and graphics pipeline, and draws a shader-driven triangle. The swapchain is recreated when SDL reports a pixel-size change. This is the renderer bootstrap; Wolfenstein level geometry is not implemented yet.

The test harness uses [Unity](https://github.com/ThrowTheSwitch/Unity), a dedicated C unit-testing framework. It contains 22 Unity test cases: three SDL3 window/event tests, four keyboard/mouse/axis input tests, 14 Vulkan selection and renderer unit tests, and one SwiftShader-backed integration test that creates a swapchain and submits/presents a triangle frame. CTest runs the suite in Windows CI.

| Check | Current status |
|---|---|
| Windows x64 build | Tracked by the Actions badge below |
| C unit-test framework | Unity |
| Automated tests | 22 Unity test cases: 3 SDL3, 4 input, 14 Vulkan unit, and 1 SwiftShader swapchain render/present integration |
| SDL3 window | Complete |
| Vulkan swapchain and triangle rendering | Implemented and verified in Windows CI |
| Input handling | Keyboard, relative mouse and SDL gamepad state; verified in Windows CI |
| Quad rendering | Next roadmap step
| Original level loading | Not started |
| Playable demo | Not started |

[![Windows build and tests](https://github.com/elaverick/WolfVK/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/elaverick/WolfVK/actions/workflows/build.yml)

The workflow configures and compiles the Windows x64 application, then runs the Unity test suite through CTest. The badge reflects the latest workflow result for the branch.

## Build locally

A C17 compiler, CMake 3.24 or newer, Git, the Vulkan SDK (including `glslc`), and network access for SDL3 and Unity source dependencies are required. Running the full test suite also requires a working Vulkan driver; CI uses SwiftShader.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Controls

The application currently displays a coloured Vulkan triangle. The input layer tracks keyboard state, relative mouse movement and a connected SDL gamepad, but gameplay controls are not wired up yet; close the window to exit.
