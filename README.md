# WolfVK

WolfVK is a modern Windows reimagining of **Wolfenstein 3D**, written in C17. It uses SDL3 for windowing and input and Vulkan for 3D rendering. The aim is to load the original game's level and wall graphics data rather than requiring replacement assets.

Development follows the ordered steps in [roadmap.md](roadmap.md). The original [Wolf3D repository](https://github.com/elaverick/Wolf3D) is a read-only reference.

## Current development status

**Current milestone:** interactive WL6 levels rendered with original wall textures, including animated doors and secret pushwalls. The application reads original-format MAPHEAD/GAMEMAPS data, expands the Carmack and RLEW compressed wall and info planes, identifies the original player-start marker and direction, generates exposed wall faces, and combines them with large floor and roof planes. The retail WL6 data contains 60 playable maps; MAPHEAD retains a 100-slot offset table.

Wall textures are loaded from `VSWAP.WL6`. The loader reads the page offsets and lengths, decodes the uncompressed 64×64 wall pages stored before the sprite-page boundary, converts their column-major palette indices to RGBA using the 256-colour Wolfenstein VGA palette, and makes them available as layers in a Vulkan texture array. The original wall-tile mapping is retained: wall type `n` selects page `(n - 1) × 2` for horizontal faces and the following page for vertical faces. Map values with the `0x40` flag are reduced to their base wall type before page selection; for example, retail tile `106` maps to pages `82` and `83`. Door IDs `90–101` remain special cells, not wall textures. The mesh UV direction follows the original raycaster's hit direction. The GPU sampler uses nearest-neighbour filtering; integer scaling is the next roadmap item and is not implemented yet.

The floor and ceiling remain flat-coloured planes. Sprites, enemies and pickups are not implemented yet. Original wall-plane door IDs 90–101 are instantiated as sliding doors using the original door page pairs from VSWAP. Normal, keyed and elevator door variants use the retail tile ID mapping; keys are collected from their original info-plane markers and the four locked-door key bits are enforced. Doors open at the original `tics << 10` rate, remain open for 300 game tics, reverse when obstructed and become traversable only once fully open. Door panels slide along their original orientation axis, use one two-sided plane to avoid coplanar depth fighting, and are removed from the draw list at full open. Secret pushwalls are identified by info-plane marker 98, can be activated while Use is held as in the original game, move at one tile per 128 game tics, reserve the next cell, stop when blocked and render using the source wall's original horizontal/vertical texture pages. Ordinary wall cells remain solid and collidable.

The player can move with **WASD** and look with the mouse. Movement is relative to camera yaw, diagonal movement is normalised, and the player is kept out of wall tiles with sliding collision resolution. Relative mouse capture is released when the window loses focus and restored when it regains focus. Press **Escape** to quit.

The application expects `MAPHEAD.WL6`, `GAMEMAPS.WL6` and `VSWAP.WL6` in the working directory. Those original game files are not included in this repository or demo package; copy them from your own WL6 installation. The user-supplied retail map files were inspected: all 60 playable maps decode as 64×64 maps, each with exactly one valid player-start marker on a floor cell. Level 1 starts at tile (29, 57), facing east.

**Textured demo:** the Windows x64 build, the SwiftShader-backed Vulkan renderer test, packaging and artifact upload passed in [Actions run 43740769232](https://github.com/elaverick/WolfVK/actions/runs/43740769232) on the earlier textured-wall milestone. The current door and secret-wall implementation has 94 registered Unity test cases. The latest Windows CI run is [43752002088](https://github.com/elaverick/WolfVK/actions/runs/43752002088) and is pending; its build and test result will supersede the earlier baseline once complete. The `WolfVK-textured-demo-windows-x64` artifact contains `wolfvk.exe`, the compiled shader files, third-party notices and setup instructions. Copy the three original WL6 files above beside the executable before launching it.

## Automated tests

The test harness uses [Unity](https://github.com/ThrowTheSwitch/Unity), a dedicated C unit-testing framework. It currently registers 94 Unity test cases (including the SwiftShader-backed renderer integration test):

- 3 SDL3 window and event tests
- 4 camera projection tests
- 11 player movement, mouse-look and collision tests, including door traversal and secret-wall pushing
- 8 command-line parser tests
- 10 Carmack/RLEW decoder and level-loader tests
- 16 wall-geometry and player-start tests, including original wall-page selection and all four face UV directions
- 7 VSWAP page decoder, palette expansion and wall-page mapping tests
- 4 keyboard, mouse and gamepad input tests
- 16 world-state tests for doors, keys, secret pushwalls and their original timing/material behaviour
- 15 Vulkan selection and renderer tests, including the SwiftShader-backed frame submission test

CTest runs the suite in Windows CI. The world-state tests cover original door orientation and locks, opening/closing speeds, the 300-tic timeout, obstruction handling, key collection, door-page selection, horizontal and vertical panel movement, fully-open panel removal, and secret pushwall movement/collision, valid texture-page selection and held-use activation.

| Feature | Status |
|---|---|
| Windows x64 build | Current door/pushwall implementation pending in [Actions run 43752002088](https://github.com/elaverick/WolfVK/actions/runs/43752002088) |
| SDL3 window and input | Implemented and tested |
| Vulkan renderer and swapchain | Implemented and tested in Windows CI |
| Indexed quad rendering and depth testing | Implemented and tested |
| Original WL6 map decoding and level selection | Implemented; 60 real WL6 maps inspected and synthetic original-format fixtures tested |
| Original VSWAP wall-page decoding | Implemented; 64×64 indexed pages converted to RGBA through the original palette |
| Original wall textures on exposed faces | Implemented; horizontal/vertical page pairing and directional UV mapping tested |
| WASD movement and mouse-look | Implemented; movement uses swept boundary resolution and wall sliding |
| Sliding doors, keys and secret pushwalls | Implemented; original WL6 tile/marker mappings and timings have regression coverage |
| Textured Windows demo | Earlier textured-wall artifact packaged by [Actions run 43740769232](https://github.com/elaverick/WolfVK/actions/runs/43740769232); refreshed artifact awaits current CI |
| Integer texture scaling | Next milestone; not implemented |

[![Windows build and tests](https://github.com/elaverick/WolfVK/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/elaverick/WolfVK/actions/workflows/build.yml)

## Build locally

A C17 compiler, CMake 3.24 or newer, Git, the Vulkan SDK (including `glslc`), and network access for the SDL3 and Unity source dependencies are required. Running the full test suite also requires a working Vulkan driver; CI uses SwiftShader.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Run

Keep `shaders/` beside `wolfvk.exe`. Place `MAPHEAD.WL6`, `GAMEMAPS.WL6` and `VSWAP.WL6` from your own Wolfenstein 3D WL6 installation in the working directory, then launch:

```powershell
.\wolfvk.exe
```

Use `--level N` to select a one-based level number; omit it to load the first level.

**Controls:** W/A/S/D to move, mouse to look, **E** or **Space** to operate a door or push marked secret wall, Escape to quit. Locked doors require their corresponding key. The mouse is captured while the window is focused.
