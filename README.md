# WolfVK

WolfVK is a modern Windows reimagining of **Wolfenstein 3D**, written in C17. It uses SDL3 for windowing and input and Vulkan for 3D rendering. The aim is to load the original game's level and wall graphics data rather than requiring replacement assets.

Development follows the ordered steps in [roadmap.md](roadmap.md). The original [Wolf3D repository](https://github.com/elaverick/Wolf3D) is a read-only reference.

## Current development status

**Current milestone:** interactive levels rendered with original wall textures. The application reads original-format MAPHEAD/GAMEMAPS data, expands the Carmack and RLEW compressed wall and info planes, identifies the original player-start marker and direction, generates exposed wall faces, and combines them with large floor and roof planes. The retail WL6 data contains 60 playable maps; MAPHEAD retains a 100-slot offset table.

Wall textures are loaded from `VSWAP.WL6`. The loader reads the page offsets and lengths, decodes the uncompressed 64×64 wall pages stored before the sprite-page boundary, converts their column-major palette indices to RGBA using the 256-colour Wolfenstein VGA palette, and makes them available as layers in a Vulkan texture array. The original wall-tile mapping is retained: wall type `n` selects page `(n - 1) × 2` for horizontal faces and the following page for vertical faces. Map values with the `0x40` flag are reduced to their base wall type before page selection; for example, retail tile `106` maps to pages `82` and `83`. Door IDs `90–101` remain special cells, not wall textures. The mesh UV direction follows the original raycaster's hit direction. The GPU sampler uses nearest-neighbour filtering; integer scaling is the next roadmap item and is not implemented yet.

The floor and ceiling remain flat-coloured planes. Sprites, enemies, pickups, animated doors, door interaction and pushwalls are not part of this milestone. Original wall-plane door IDs 90–101 and secret-door cells marked by info-plane value 98 are omitted from the static wall mesh and treated as passable openings; ordinary wall cells remain solid and collidable.

The player can move with **WASD** and look with the mouse. Movement is relative to camera yaw, diagonal movement is normalised, and the player is kept out of wall tiles with sliding collision resolution. Relative mouse capture is released when the window loses focus and restored when it regains focus. Press **Escape** to quit.

The application expects `MAPHEAD.WL6`, `GAMEMAPS.WL6` and `VSWAP.WL6` in the working directory. Those original game files are not included in this repository or demo package; copy them from your own WL6 installation. The user-supplied retail map files were inspected: all 60 playable maps decode as 64×64 maps, each with exactly one valid player-start marker on a floor cell. Level 1 starts at tile (29, 57), facing east.

**Textured demo:** the Windows x64 build, all 78 registered Unity tests, the SwiftShader-backed Vulkan integration test, packaging and artifact upload passed in [Actions run 43740769232](https://github.com/elaverick/WolfVK/actions/runs/43740769232). The `WolfVK-textured-demo-windows-x64` artifact contains `wolfvk.exe`, the compiled shader files, third-party notices and setup instructions. Copy the three original WL6 files above beside the executable before launching it.

## Automated tests

The test harness uses [Unity](https://github.com/ThrowTheSwitch/Unity), a dedicated C unit-testing framework. It contains 78 registered Unity test cases:

- 3 SDL3 window and event tests
- 4 camera projection tests
- 11 player movement, mouse-look and collision tests, including ordinary and secret door traversal
- 8 command-line parser tests
- 10 Carmack/RLEW decoder and level-loader tests
- 16 wall-geometry and player-start tests, including original wall-page selection and all four face UV directions
- 7 VSWAP page decoder, palette expansion and wall-page mapping tests
- 4 keyboard, mouse and gamepad input tests
- 14 Vulkan selection and renderer unit tests
- 1 SwiftShader-backed integration test that uploads a wall texture array, samples a textured quad, and submits/presents a frame

CTest runs the suite in Windows CI.

| Feature | Status |
|---|---|
| Windows x64 build | Passed on commit `4076af5`; all 78 Unity cases passed |
| SDL3 window and input | Implemented and tested |
| Vulkan renderer and swapchain | Implemented and tested in Windows CI |
| Indexed quad rendering and depth testing | Implemented and tested |
| Original WL6 map decoding and level selection | Implemented; 60 real WL6 maps inspected and synthetic original-format fixtures tested |
| Original VSWAP wall-page decoding | Implemented; 64×64 indexed pages converted to RGBA through the original palette |
| Original wall textures on exposed faces | Implemented; horizontal/vertical page pairing and directional UV mapping tested |
| WASD movement and mouse-look | Implemented; player collision tests pass |
| Textured Windows demo | Packaged and uploaded by [Actions run 43740769232](https://github.com/elaverick/WolfVK/actions/runs/43740769232) |
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

**Controls:** W/A/S/D to move, mouse to look, Escape to quit. The mouse is captured while the window is focused.
