# WolfVK

WolfVK is a modern Windows reimagining of **Wolfenstein 3D**, written in C17. It uses SDL3 for windowing and input and Vulkan for 3D rendering. The aim is to load the original game's level and wall graphics data rather than requiring replacement assets.

Development follows the ordered steps in [roadmap.md](roadmap.md). The original [Wolf3D repository](https://github.com/elaverick/Wolf3D) is a read-only reference.

## Current development status

**Current milestone:** interactive WL6 levels with original wall textures, animated doors, secret pushwalls, voxel actors/objects and first-person voxel weapons. The application reads original-format MAPHEAD/GAMEMAPS data, expands the Carmack and RLEW compressed wall and info planes, identifies the original player-start marker and direction, generates exposed wall faces, and combines them with large floor and roof planes. The retail WL6 data contains 60 playable maps; MAPHEAD retains a 100-slot offset table.

Wall textures are loaded from `VSWAP.WL6`. The loader reads the page offsets and lengths, decodes the uncompressed 64×64 wall pages stored before the sprite-page boundary, converts their column-major palette indices to RGBA using the 256-colour Wolfenstein VGA palette, and makes them available as layers in a Vulkan texture array. The original wall-tile mapping is retained: wall type `n` selects page `(n - 1) × 2` for horizontal faces and the following page for vertical faces. Map values with the `0x40` flag are reduced to their base wall type before page selection. Original `AMBUSHTILE` value 106 is removed from the wall map after info-plane processing, while area markers 107 and above are floor. Door IDs `90–101` remain special cells, not wall textures. The mesh UV direction follows the original raycaster's hit direction. The GPU sampler uses nearest-neighbour filtering; integer scaling is the next roadmap item and is not implemented yet.

The floor and ceiling remain flat-coloured planes, now using neutral greys measured from the supplied original-game reference capture (floor RGB 113,113,113; ceiling RGB 56,56,56). Their vertex colours are specified in linear space so the sRGB swapchain reproduces those display values. Actors and objects are drawn as voxel models (see [Voxel sprites](#voxel-sprites)); they stand still in their spawn pose for now, with no animation or AI. The four first-person weapons are separately loaded as voxel models and drawn in a camera-attached Vulkan overlay. The overlay clears the world's depth values before drawing, then uses its own depth testing so the weapon always appears over the level while nearer weapon voxels correctly occlude farther ones. Original wall-plane door IDs 90–101 are instantiated as sliding doors using the original door page pairs from VSWAP. Normal, keyed and elevator door variants use the retail tile ID mapping; keys are collected from their original info-plane markers and the four locked-door key bits are enforced. Doors open at the original `tics << 10` rate, remain open for 300 game tics, reverse when obstructed and become traversable only once fully open. Door panels slide along their original orientation axis, use one two-sided plane to avoid coplanar depth fighting, and are removed from the draw list at full open. Secret pushwalls are identified by info-plane marker 98, can be activated while Use is held as in the original game, move at one tile per 128 game tics, reserve the next cell, stop when blocked and render using the source wall's original horizontal/vertical texture pages. Original ambush marker value 106 is cleared from the wall map, preventing phantom blocks from obstructing the player or pushwall path. The supplied Level 1 map has five secret markers on wall types 1, 4 and 10; their original page pairs (0/1, 6/7 and 18/19) are covered by a dedicated regression test. Ordinary wall cells remain solid and collidable.

### Voxel sprites

Every actor and static object that the original `ScanInfoPlane` would spawn is drawn as a solid voxel model built from its sprite by `tools/sprite_voxelizer`. The engine reads one MagicaVoxel file per subject (`grd.vox`, `stat_12.vox` ...) from an `assets/voxels` directory beside the WL6 files, or beside the executable. These files are derived from your own game data, so they are not in the repository; create them with the voxelizer's `--assets` option (see [Run](#run)). If the directory is missing, the game reports it and runs without actors, objects or weapons.

- **What is placed:** info-plane markers 23–71 become static objects `stat_0` … `stat_47` (71 is the second ammunition clip, which reuses `SPR_STAT_26`). Guards, officers, SS, dogs and mutants use their standing pose (`grd_s`) or, when patrolling, the first stride of their walk (`grd_w` pose 0), as the original `s_*stand` and `s_*path1` states do. Bosses and ghosts stand in the first frame of their walk; marker 178 places Hitler in his mech suit, and marker 124 is the dead guard.
- **Difficulty:** as in the original, the second and third tiers of enemy markers spawn only at "Bring 'em on!" and "I am Death incarnate!". `--difficulty 1-4` picks the level in menu order; the default is 3, the original menu's default.
- **Facing:** eight-way actors keep the compass direction the map gives them. Everything else was a sprite that always faced the player. A solid model needs a fixed facing, so these face along their longest straight run of open floor, which turns them away from nearby walls.
- **Scale and placement:** one voxel is one sprite pixel, so 64 voxels span a tile and a full-height model reaches the ceiling. Models stand at the centre of their tile on the floor.
- **Rendering:** each distinct pose a level uses is meshed once. Only faces between a voxel and empty space are kept, and same-coloured neighbours are merged into larger rectangles. Faces on the floor or against the ceiling can never be seen, so they are dropped. Colours come straight from the original palette, converted to linear space for the sRGB swapchain, with a fixed brightness per face direction so shapes read clearly without lighting. The meshes live in their own vertex buffer. Each placed model is one indexed draw with its own model-view-projection push constant, so the shaders are unchanged and door animation never re-copies model vertices. Later animation only has to point an instance at a different pose. Level 1 places 142 objects at the default difficulty, using 31 poses and about 42,000 quads.
- **First-person weapons:** `knife.vox`, `pistol.vox`, `machinegun.vox` and `chain.vox` contain the voxeliser's `<subject>_ready` and `<subject>_atk` scene objects (for example, `knife_ready` and `knife_atk`). Each weapon's ready pose and four attack poses are loaded from those objects. The separate `player_*` names in the voxeliser manifest describe the original game's timed state sequences, not VOX scene objects. The weapon model is transformed relative to camera position, yaw and pitch, then drawn after the world models using a cleared depth buffer: nearby walls cannot hide it, while its own near-side voxels correctly occlude the far-side geometry. Missing weapon poses are reported but do not prevent the rest of the game from running.
- **Firing:** the original four six-tic attack states run at 70 tics per second. The pistol and knife return to ready after one attack; the automatic weapons repeat their original attack loop while fire is held. A firing kick is applied to the view model. Hit detection, projectiles, damage, ammunition and weapon pickups are deliberately not implemented yet.
- **Collected keys** disappear because the placed models are rebuilt from the info plane when a key is picked up. Other pickups cannot be collected yet, and static objects and actors do not block movement yet.

All 60 retail WL6 levels were loaded at the hardest difficulty, which places every enemy tier, and every pose they need was available.

The player can move with **WASD** and look with the mouse. Movement is relative to camera yaw, diagonal movement is normalised, and the player is kept out of wall tiles with sliding collision resolution. Use **1**, **2**, **3** or **4** to select the knife, pistol, machine gun or chaingun. Hold the **left mouse button** or **Left Ctrl** to fire; a gamepad's **right shoulder** or **right trigger** also fires. Relative mouse capture is released when the window loses focus and restored when it regains focus. Press **Escape** to quit.

The application expects `MAPHEAD.WL6`, `GAMEMAPS.WL6` and `VSWAP.WL6` in the working directory. Those original game files are not included in this repository or demo package; copy them from your own WL6 installation. The user-supplied retail map files were inspected: all 60 playable maps decode as 64×64 maps, each with exactly one valid player-start marker on a floor cell. Level 1 starts at tile (29, 57), facing east.

**Demo:** the previous actor-and-object milestone at `960ccad` passed CI in [Actions run 43931333985](https://github.com/elaverick/WolfVK/actions/runs/43931333985) on 16 November 2026. That run covered the Windows x64 build, all 128 then-registered Unity tests (including SwiftShader-backed Vulkan rendering), the sprite voxelizer tests on Windows and Linux, and packaging. The current weapon milestone is committed on `main` at `8cb74ca` (source and tests are included in that revision). Its Windows build and 140-test result have not yet been reported by the connected GitHub status reader. The previously verified `WolfVK-textured-demo-windows-x64` artifact predates the weapon changes; a new package awaits a successful workflow run. Copy the three original WL6 files above beside the executable, run the voxelizer with `--assets assets\voxels` as the instructions describe, and then launch the game.

## Automated tests

The test harness uses [Unity](https://github.com/ThrowTheSwitch/Unity), a dedicated C unit-testing framework. It currently registers 142 Unity test cases (including the SwiftShader-backed renderer integration tests). The weapon milestone adds eleven weapon controller, camera-transform and VOX-loader cases; the mouse-button tracking test adds one more to the previous 128-case suite:

- 3 SDL3 window and event tests
- 4 camera projection tests
- 11 player movement, mouse-look and collision tests, including door traversal and secret-wall pushing
- 11 command-line parser tests, including `--difficulty`
- 7 MagicaVoxel reader tests: named animations and keyframed poses, the stored palette, oversized models, truncated or foreign data and dangling model references, against synthetic files laid out as the voxelizer writes them
- 6 voxel mesher tests: hidden floor and ceiling faces, face merging by colour, model-space orientation and linear, shaded colour
- 7 actor and object placement tests: static markers, standing and patrolling facings, difficulty tiers, bosses, ghosts, the dead guard and open-floor facing
- 4 voxel scene tests: one mesh per shared pose, missing files and poses, and tile placement
- 11 Carmack/RLEW decoder and level-loader tests
- 18 wall-geometry and player-start tests, including original wall-page selection, door-side textures and all four face UV directions
- 7 VSWAP page decoder, palette expansion and wall-page mapping tests
- 5 keyboard, mouse and gamepad input tests, including held mouse-button state
- 18 world-state tests for doors, keys, secret pushwalls and their original timing/material behaviour
- 17 Vulkan selection and renderer tests, including the SwiftShader-backed frame submission test, the model transform, and a frame that draws placed model instances
- 13 weapon tests: original 70 Hz/six-tic attack timing, automatic repeat, weapon selection, pose selection, view-model anchoring, all 20 named VOX poses and partial-asset fallback

CTest runs the suite in Windows CI. The world-state tests cover original door orientation and locks, opening/closing speeds, the 300-tic timeout, obstruction handling, key collection, door-page selection, horizontal and vertical panel movement, fully-open panel removal, and secret pushwall movement/collision, valid texture-page selection, held-use activation, the Level 1 secret-wall page pairs and ambush-marker removal.

| Feature | Status |
|---|---|
| Windows x64 build | Previous milestone passed in [Actions run 43931333985](https://github.com/elaverick/WolfVK/actions/runs/43931333985); current weapon revision is committed and awaiting its Windows CI result |
| SDL3 window and input | Implemented and tested; left mouse button state covered by a regression test |
| Vulkan renderer and swapchain | Implemented; weapon overlay pipeline added, current revision awaiting Windows CI |
| Indexed quad rendering and depth testing | Implemented and tested |
| Original WL6 map decoding and level selection | Implemented; 60 real WL6 maps inspected and synthetic original-format fixtures tested |
| Original VSWAP wall-page decoding | Implemented; 64×64 indexed pages converted to RGBA through the original palette |
| Original wall textures on exposed faces | Implemented; horizontal/vertical page pairing and directional UV mapping tested |
| WASD movement and mouse-look | Implemented; movement uses swept boundary resolution and wall sliding |
| Sliding doors, keys and secret pushwalls | Implemented; original WL6 tile/marker mappings and timings have regression coverage |
| Voxel actors and static objects | Implemented: placed by the original info-plane and difficulty rules, standing in their spawn pose. Verified locally on all 60 retail levels and passed in [Actions run 43931333985](https://github.com/elaverick/WolfVK/actions/runs/43931333985). Animation, AI, pickups and object collision come later |
| Player voxel weapons | Implemented: knife, pistol, machine gun and chaingun, camera-anchored overlay, original attack timing, keyboard/mouse/gamepad firing. Eleven weapon tests added; current CI result pending. No projectile interaction yet |
| Sprite voxelizer | Builds and passes its tests on Windows and Linux in [Actions run 43931333985](https://github.com/elaverick/WolfVK/actions/runs/43931333985) |
| Windows demo | Packaged with the game, the voxelizer and the shaders by [Actions run 43931333985](https://github.com/elaverick/WolfVK/actions/runs/43931333985) |
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

Keep `shaders/` beside `wolfvk.exe`. Place `MAPHEAD.WL6`, `GAMEMAPS.WL6` and `VSWAP.WL6` from your own Wolfenstein 3D WL6 installation in the working directory. To see actors, objects and first-person weapons, build the [sprite voxelizer](tools/sprite_voxelizer/README.md) once and install its models beside the game data:

```powershell
build-tools\Release\wolf_sprite_voxelizer.exe --vswap VSWAP.WL6 --output tools\sprite_voxelizer\output --assets <game directory>\assets\voxels
```

The game directory then looks like this:

```
wolfvk.exe
shaders\            triangle.vert.spv, triangle.frag.spv
MAPHEAD.WL6
GAMEMAPS.WL6
VSWAP.WL6
assets\voxels\      grd.vox, dog.vox, ss.vox, stat_0.vox ... (77 files)
```

Then launch:

```powershell
.\wolfvk.exe
```

Use `--level N` to select a one-based level number; omit it to load the first level. Use `--difficulty N` (1–4, in the original menu's order) to choose which enemies spawn; omit it for "Bring 'em on!".

**Controls:** W/A/S/D to move, mouse to look, **E** or **Space** to operate a door or push marked secret wall, **1–4** to select a weapon, **left mouse button** or **Left Ctrl** to fire (gamepad right shoulder/trigger also fires), Escape to quit. Locked doors require their corresponding key. The mouse is captured while the window is focused. Firing is visual only at this stage.

## Tools

`tools/sprite_voxelizer` is a standalone offline tool, not part of the game build. It extracts all 436 WL6 sprites from `VSWAP.WL6`, removes painted floor shadows, and rebuilds every pose as a 64×64×64 MagicaVoxel model. Eight-way rotation sets are carved into one solid; camera-facing frames get depth from the actor's own body or a fitted profile. Each actor or object is written as a single `.vox` file holding all of its animations, and every model is validated against the original frames. A dependency-free HTML viewer (`tools/sprite_voxelizer/viewer`) shows the results in 3D. `--assets <dir>` installs the subject files where the game loads them. See [its README](tools/sprite_voxelizer/README.md). The output is derived from your own game data and is not committed.

