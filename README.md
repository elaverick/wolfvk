# WolfVK

WolfVK is a modern Windows reimagining of **Wolfenstein 3D**, written in C17. It uses SDL3 for windowing and input and Vulkan for 3D rendering. The aim is to load the original game's level and graphic data rather than requiring replacement assets.

Development follows the ordered, approval-gated steps in [roadmap.md](roadmap.md). The original [Wolf3D repository](https://github.com/elaverick/Wolf3D) is a read-only reference.

## Current development status

**Current milestone:** interactive, untextured level rendering. The SDL3 window and Vulkan renderer draw indexed, vertex-coloured quads with depth testing and a perspective camera. The application reads original-format MAPHEAD/GAMEMAPS data, expands the Carmack and RLEW compressed wall and info planes, identifies the original player-start marker and direction, generates exposed wall faces, and combines them with large floor and roof planes. The retail WL6 data contains 60 playable maps; MAPHEAD retains a 100-slot offset table.

The player can move with **WASD** and look with the mouse. Movement is relative to camera yaw, diagonal movement is normalised, and the player is kept out of wall tiles with sliding collision resolution. Relative mouse capture is released when the window loses focus and restored when it regains focus. Press **Escape** to quit.

The level loader expects `MAPHEAD.WL6` and `GAMEMAPS.WL6` in the working directory. Those original game files are not included in this repository. The uploaded retail WL6 files have been inspected: all 60 playable maps decode as 64×64 maps, each with exactly one valid player-start marker on a floor cell. Level 1 starts at tile (29, 57), facing east. This data inspection validates the map structure and spawn interpretation; the live Vulkan rendering still needs visual confirmation.

The approval gate before texture work is the next milestone: build and present a Windows demo for review. Textures must not be added until that demo has been confirmed.

## Automated tests

The test harness uses [Unity](https://github.com/ThrowTheSwitch/Unity), a dedicated C unit-testing framework. It contains 61 Unity test cases:

- 3 SDL3 window and event tests
- 3 camera projection tests
- 9 player movement, mouse-look and collision tests
- 4 keyboard, mouse and gamepad input tests
- 10 Carmack/RLEW decoder and level-loader tests
- 9 wall-geometry and player-start tests
- 8 command-line parser tests
- 14 Vulkan selection and renderer unit tests
- 1 SwiftShader-backed integration test that creates a swapchain and submits/presents overlapping indexed quads

CTest runs the suite in Windows CI.

| Feature | Status |
|---|---|
| Windows x64 build | Verified by GitHub Actions |
| SDL3 window and input | Implemented and tested |
| Vulkan renderer and swapchain | Implemented and tested in Windows CI |
| Indexed quad rendering and depth testing | Implemented and tested |
| Infinite floor and roof planes | Implemented |
| Original WL6 map decoding and level selection | Implemented; 60 real WL6 maps inspected and synthetic original-format fixtures tested |
| Untextured exposed wall faces | Implemented; level 1 geometry data yields 1,016 exposed wall faces; visual check outstanding |
| WASD movement and mouse-look | Implemented; player collision tests pass |
| Pre-texture demo | Next approval-gated milestone |
| Original textures and integer scaling | Not started; wait for demo approval |

[![Windows build and tests](https://github.com/elaverick/WolfVK/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/elaverick/WolfVK/actions/workflows/build.yml)

## Build locally

A C17 compiler, CMake 3.24 or newer, Git, the Vulkan SDK (including `glslc`), and network access for the SDL3 and Unity source dependencies are required. Running the full test suite also requires a working Vulkan driver; CI uses SwiftShader.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Run

Keep `shaders/` beside `wolfvk.exe`. Place `MAPHEAD.WL6` and `GAMEMAPS.WL6` from your own Wolfenstein 3D WL6 installation in the same working directory, then launch:

```powershell
.\wolfvk.exe
```

Use `--level N` to select a one-based level number; omit it to load the first level.

**Controls:** W/A/S/D to move, mouse to look, Escape to quit. The mouse is captured while the window is focused.
