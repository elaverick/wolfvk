# WolfVK

WolfVK is a modern Windows reimagining of **Wolfenstein 3D**, written in C17. It uses SDL3 for windowing and input and Vulkan for 3D rendering. The aim is to load the original game's level and graphic data rather than requiring replacement assets.

Development follows the ordered, approval-gated steps in [roadmap.md](roadmap.md). The original [Wolf3D repository](https://github.com/elaverick/Wolf3D) is a read-only reference.

## Current development status

**Current milestone:** interactive, untextured level rendering. The SDL3 window and Vulkan renderer draw indexed, vertex-coloured quads with depth testing and a perspective camera. The application reads original-format MAPHEAD/GAMEMAPS data, expands the Carmack and RLEW compressed wall and info planes, identifies the original player-start marker and direction, generates exposed wall faces, and combines them with large floor and roof planes. The retail WL6 data contains 60 playable maps; MAPHEAD retains a 100-slot offset table.

The player can move with **WASD** and look with the mouse. Movement is relative to camera yaw, diagonal movement is normalised, and the player is kept out of wall tiles with sliding collision resolution. Relative mouse capture is released when the window loses focus and restored when it regains focus. Press **Escape** to quit.

Door handling is deliberately minimal at this milestone. Original wall-plane door IDs 90–101 and secret-door cells marked by info-plane value 98 are omitted from the static wall mesh and treated as passable openings. This prevents an unrendered door becoming an invisible collision barrier. Ordinary wall cells remain solid; door animation, interaction and locking are not implemented yet.

The level loader expects `MAPHEAD.WL6` and `GAMEMAPS.WL6` in the working directory. Those original game files are not included in this repository or demo package; copy them from your own WL6 installation beside `wolfvk.exe`. The user-supplied retail WL6 files were inspected: all 60 playable maps decode as 64×64 maps, each with exactly one valid player-start marker on a floor cell. Level 1 starts at tile (29, 57), facing east. The player spawn uses this original marker and direction. The actual untextured Windows demo is packaged and ready for visual review. The current build also omits normal and secret door cells from geometry and collision, while retaining ordinary wall collision.

**Demo review:** the Windows x64 pre-texture demo has been built, passed the complete Windows CI test suite, and is available from the [latest successful Actions run](https://github.com/elaverick/WolfVK/actions/runs/43726924265) under **Artifacts** (`WolfVK-demo-windows-x64`). A copy is also provided with this handoff. Please review the untextured level and movement. Original texture loading and integer scaling remain blocked until you explicitly approve this demo.

## Automated tests

The test harness uses [Unity](https://github.com/ThrowTheSwitch/Unity), a dedicated C unit-testing framework. It contains 67 registered Unity test cases:

- 3 SDL3 window and event tests
- 3 camera projection tests
- 11 player movement, mouse-look and collision tests, including ordinary and secret door traversal
- 4 keyboard, mouse and gamepad input tests
- 10 Carmack/RLEW decoder and level-loader tests
- 13 wall-geometry and player-start tests, including ordinary and secret door omission
- 8 command-line parser tests
- 14 Vulkan selection and renderer unit tests
- 1 SwiftShader-backed integration test that creates a swapchain and submits/presents overlapping indexed quads

CTest runs the suite in Windows CI.

| Feature | Status |
|---|---|
| Windows x64 build | Passed on commit `8a7202b`; all 67 registered Unity cases passed |
| SDL3 window and input | Implemented and tested |
| Vulkan renderer and swapchain | Implemented and tested in Windows CI |
| Indexed quad rendering and depth testing | Implemented and tested |
| Infinite floor and roof planes | Implemented |
| Original WL6 map decoding and level selection | Implemented; 60 real WL6 maps inspected and synthetic original-format fixtures tested |
| Untextured exposed wall faces | Implemented; level 1 geometry data yields 1,016 exposed wall faces; ordinary doors (wall-plane 90–101) and secret doors (info-plane 98) are omitted as passable openings; visual check outstanding |
| WASD movement and mouse-look | Implemented; player collision tests pass |
| Pre-texture demo | Packaged and available for visual review; awaiting approval |
| Original textures and integer scaling | Not started; require explicit demo approval |

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
