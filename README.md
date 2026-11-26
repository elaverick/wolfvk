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
- **Firing:** follows the original `Cmd_Fire` and `T_Attack`. Each weapon's four attack frames last six tics at 70 tics per second, and a press of fire starts one attack; holding it does not start another. The pistol fires once per press, on its muzzle-flash frame. The machine gun fires every 12 tics while fire is held, and the chaingun every 6 tics (and twice even for a single tap). Every shot uses one bullet from the player state, so the status bar counts down. When an attack ends with no bullets left the knife comes out; picking up ammunition brings the chosen gun back. The weapon cannot be changed part-way through an attack. The knife uses no ammunition. A firing kick is applied to the view model. Shots and knife strikes are reported as events but do no damage yet; that comes with the enemy work.
- **Pickups** follow the original `GetBonus` (see [Pickups](#pickups)); a picked-up item disappears because the placed models are rebuilt from the info plane. Static objects use the original WL6 `statinfo[]` block flags for movement collision: tables, barrels, armour and other `block` entries stop the player, while puddles, floor skeletons, the dead guard and pickups remain passable. Living actors do not yet participate in movement collision.

All 60 retail WL6 levels were loaded at the hardest difficulty, which places every enemy tier, and every pose they need was available.

### Spatial audio

The engine uses SDL3_mixer for positional playback. Each effect gets an independent mixer track, so concurrent weapon and world sounds do not replace one another. The listener follows the player camera, and world-space emitters are transformed into listener-relative coordinates before being passed to SDL3_mixer. Weapon-fire events and pickup events are sent through this common sound interface; future enemy sounds can use the same API with the enemy's world position.

Extracted sound files are loaded by their original WL6 sound names from `assets/sounds`. Run the [WL6 audio extractor](tools/audio_extractor/README.md) against your own `VSWAP.WL6` file to produce OGG Vorbis assets. FFmpeg with `libvorbis` is required by the extractor. The current converter handles the digitised sample effects listed in the original `DigiMap`; PC-speaker and AdLib-only sound effects, plus the 27 AdLib music tracks, still require OPL synthesis and are not yet converted. The `assets/music` folder is reserved for those music OGGs.

### Status bar

The original status bar is drawn along the bottom of the window from the game's own pictures in `VGAGRAPH.WL6`, using `VGAHEAD.WL6` for the chunk offsets and `VGADICT.WL6` for the Huffman tree. The pictures are decompressed and reordered from the original four-plane Mode X layout at startup.

- **Contents:** the bar is composed exactly as `DrawPlayScreen` and the `Draw*` routines in `WL_AGENT.C` place it: floor, score, lives, BJ's face, health, ammunition, gold and silver key slots and the current weapon, with numbers right-aligned and blank-padded as `LatchNumber` does. The face shows one of seven damage levels, chosen from health as in `DrawFace`, and glances left and right using the original `UpdateFace` timing: a fresh random 0–255 threshold is tested once per 70 Hz game tic against the accumulated face counter, independent of display refresh rate.
- **Real values:** the numbers come from a player state (`include/wolfvk/player_state.h`) modelled on the original `gamestate`. A new game starts as `NewGame` does, with 100 health, 8 bullets, 3 lives and the pistol, and the floor shown is the map within its episode. Health, ammunition, weapons, points and extra lives each go through one function that applies the original limits. Keys are held there too, so the key the player picks up is the one door locks check and the one the bar shows. Pickups change these values, and so the bar, as they are collected. Damage arrives with enemy AI and will change the bar through the same functions.
- **Integer scaling:** the bar is drawn at the largest whole-number scale at which a full 320×200 screen would fit the window (3× in the default 1280×720 window), with nearest-neighbour sampling, so every original pixel becomes an exact square block. It is centred, and the rest of its strip is filled with the original play-screen border colour (palette index 127).
- **View placement:** the 3D view is drawn only in the area above the bar, using that area's aspect ratio, so the bar never hides any of the scene.
- **Updates:** the bar is redrawn each frame on the CPU and sent to the GPU only when something on it has changed.

If the three VGA files are not beside the map files, the game says so and runs without a bar, with the view filling the window.

### Pickups

Every bonus object in the WL6 `statinfo` table can be picked up (`src/bonus_items.c`), with the effects of the original `GetBonus`:

| Item | Effect |
|---|---|
| Gold and silver keys | Open the matching locked doors |
| First aid, dinner, dog food | 25, 10 and 4 health; left on the floor at full health |
| Blood and bones (gibs) | 1 health, but only when health is 10 or less |
| Ammunition clip / a guard's dropped clip | 8 and 4 bullets, up to 99; left on the floor at 99 |
| Machine gun, chaingun | 6 bullets, and switches to the new weapon if it is better; the chaingun makes BJ grin on the status bar |
| Cross, chalice, bible, crown | 100, 500, 1,000 and 5,000 points, counted as treasure |
| One-up | Full health, 25 bullets and an extra life, counted as treasure |

Points award an extra life at every 40,000, up to nine lives. Items are grabbed as in the original's `TransformTile` test: when the item's tile centre is just ahead of the player (less than about two thirds of a tile) and within half a tile to either side, so the player picks things up by walking towards them, not by backing over them.

A new game starts with only the knife and the pistol and the original 8 bullets. Keys **1**–**4** choose only among weapons already picked up, and with no bullets the weapon cannot be changed, as in `CheckWeaponChange`. Each shot uses one bullet (see **Firing** above).

The player can move with **WASD** and look with the mouse. Movement is relative to camera yaw, diagonal movement is normalised, and the player is kept out of wall tiles with sliding collision resolution. Use **1**, **2**, **3** or **4** to select the knife, pistol, machine gun or chaingun once they have been picked up; a new game has the knife and pistol. Hold the **left mouse button** or **Left Ctrl** to fire; a gamepad's **right shoulder** or **right trigger** also fires. Relative mouse capture is released when the window loses focus and restored when it regains focus. Press **Escape** to quit.

The application expects `MAPHEAD.WL6`, `GAMEMAPS.WL6` and `VSWAP.WL6` in the working directory. Those original game files are not included in this repository or demo package; copy them from your own WL6 installation. The user-supplied retail map files were inspected: all 60 playable maps decode as 64×64 maps, each with exactly one valid player-start marker on a floor cell. Level 1 starts at tile (29, 57), facing east.

**Demo:** the previous actor-and-object milestone at `960ccad` passed CI in [Actions run 43931333985](https://github.com/elaverick/WolfVK/actions/runs/43931333985) on 16 November 2026. That run covered the Windows x64 build, all 128 then-registered Unity tests (including SwiftShader-backed Vulkan rendering), the sprite voxelizer tests on Windows and Linux, and packaging. The current weapon milestone is committed on `main` at `8cb74ca` (source and tests are included in that revision). Its Windows build and 140-test result have not yet been reported by the connected GitHub status reader. The previously verified `WolfVK-textured-demo-windows-x64` artifact predates the weapon changes; a new package awaits a successful workflow run. Copy the three original WL6 files above beside the executable, run the voxelizer with `--assets assets\voxels` as the instructions describe, and then launch the game.

## Automated tests

The test harness uses [Unity](https://github.com/ThrowTheSwitch/Unity), a dedicated C unit-testing framework. The game test harness currently registers 176 Unity test cases (including the SwiftShader-backed renderer integration tests). The standalone audio-extractor harness adds six more tests for VSWAP page reconstruction, malformed archive handling and original DigiMap aliases. The weapon milestone adds eleven weapon controller, camera-transform and VOX-loader cases; the mouse-button tracking test adds one more to the previous 128-case suite, the status bar adds eighteen VGA decoder, player-state, status bar, layout and renderer cases, pickups add six, and firing adds five:

- 3 SDL3 window and event tests
- 4 camera projection tests
- 13 player movement, mouse-look and collision tests, including original static-object blocking, door traversal and secret-wall pushing
- 11 command-line parser tests, including `--difficulty`
- 7 MagicaVoxel reader tests: named animations and keyframed poses, the stored palette, oversized models, truncated or foreign data and dangling model references, against synthetic files laid out as the voxelizer writes them
- 6 voxel mesher tests: hidden floor and ceiling faces, face merging by colour, model-space orientation and linear, shaded colour
- 8 actor and object placement tests: static markers, original WL6 static-object collision flags, standing and patrolling facings, difficulty tiers, bosses, ghosts, the dead guard and open-floor facing
- 4 voxel scene tests: one mesh per shared pose, missing files and poses, and tile placement
- 11 Carmack/RLEW decoder and level-loader tests
- 18 wall-geometry and player-start tests, including original wall-page selection, door-side textures and all four face UV directions
- 7 VSWAP page decoder, palette expansion and wall-page mapping tests
- 5 keyboard, mouse and gamepad input tests, including held mouse-button state
- 5 VGAGRAPH decoder tests: Huffman expansion, Mode X plane order, sparse chunks and malformed files
- 8 player-state tests: the original new-game values, health, ammunition, weapon, key and extra-life rules, plus face-glance timing
- 6 pickup tests: the statinfo bonus table, the original grab range, every item's effect, items left on the floor when not needed, and weapon choice limited to weapons picked up
- 6 status bar tests: every number, key, face and weapon position, the face for each damage level, palette expansion and the integer-scale layout
- 18 world-state tests for doors, keys, secret pushwalls and their original timing/material behaviour
- 18 Vulkan selection and renderer tests, including the SwiftShader-backed frame submission test, the model transform, a frame that draws placed model instances, and frames that draw and re-upload the status bar
- 18 weapon tests: original 70 Hz/six-tic attack timing, automatic repeat, weapon selection, pose selection, view-model anchoring, all 20 named VOX poses and partial-asset fallback, plus firing: the shot frame, one bullet per shot, a fresh press per pistol or knife attack, machine gun and chaingun fire rates, and the switch to the knife when the bullets run out

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
| Voxel actors and static objects | Implemented: placed by the original info-plane and difficulty rules, standing in their spawn pose. Verified locally on all 60 retail levels and passed in [Actions run 43931333985](https://github.com/elaverick/WolfVK/actions/runs/43931333985). Pickups are implemented; animation and AI come later |
| Spatial audio | SDL3_mixer voice pool and listener-relative spatialisation are wired into weapon and pickup events. Runtime playback requires extracted OGG assets in `assets/sounds`; full AdLib/PC-speaker and music conversion remains outstanding |
| WL6 audio extractor | Implemented for the digitised VSWAP sound samples; six standalone tests cover archive decoding and DigiMap naming. Uses FFmpeg/libvorbis to write OGG. AdLib-only effects and the 27 music tracks are not yet rendered |
| Player voxel weapons | Implemented: knife, pistol, machine gun and chaingun, camera-anchored overlay, original attack timing, keyboard/mouse/gamepad firing. Eleven weapon tests added; current CI result pending. Firing uses ammunition as in `T_Attack`; no damage yet |
| Sprite voxelizer | Builds and passes its tests on Windows and Linux in [Actions run 43931333985](https://github.com/elaverick/WolfVK/actions/runs/43931333985) |
| Windows demo | Packaged with the game, the voxelizer and the shaders by [Actions run 43931333985](https://github.com/elaverick/WolfVK/actions/runs/43931333985) |
| Original status bar | Implemented: decoded from VGAGRAPH, drawn at an integer scale below the view and driven by the player state; original 70 Hz random face-glance timing covered by regression tests |
| Integer texture scaling | Next milestone; not implemented for the 3D scene (the status bar already uses integer scaling) |

[![Windows build and tests](https://github.com/elaverick/WolfVK/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/elaverick/WolfVK/actions/workflows/build.yml)

## Build locally

A C17 compiler, CMake 3.24 or newer, Git, the Vulkan SDK (including `glslc`), and network access for the SDL3 and Unity source dependencies are required. Running the full test suite also requires a working Vulkan driver; CI uses SwiftShader.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Run

Keep `shaders/` beside `wolfvk.exe`. Place `MAPHEAD.WL6`, `GAMEMAPS.WL6` and `VSWAP.WL6` from your own Wolfenstein 3D WL6 installation in the working directory, with `VGAHEAD.WL6`, `VGADICT.WL6` and `VGAGRAPH.WL6` for the status bar. To see actors, objects and first-person weapons, build the [sprite voxelizer](tools/sprite_voxelizer/README.md) once and install its models beside the game data:

```powershell
build-tools\Release\wolf_sprite_voxelizer.exe --vswap VSWAP.WL6 --output tools\sprite_voxelizer\output --assets <game directory>\assets\voxels
build-audio\Release\wolf_audio_extractor.exe --vswap VSWAP.WL6 --output assets\sounds
```

The audio command requires FFmpeg with libvorbis in `PATH`. The game directory then looks like this:

```
wolfvk.exe
shaders\            triangle.vert.spv, triangle.frag.spv
MAPHEAD.WL6
GAMEMAPS.WL6
VSWAP.WL6
VGAHEAD.WL6
VGADICT.WL6
VGAGRAPH.WL6
assets\voxels\      grd.vox, dog.vox, ss.vox, stat_0.vox ... (77 files)
assets\sounds\      ATKPISTOLSND.ogg, ATKMACHINEGUNSND.ogg, ...
assets\music\       reserved for converted music tracks
```

Then launch:

```powershell
.\wolfvk.exe
```

Use `--level N` to select a one-based level number; omit it to load the first level. Use `--difficulty N` (1–4, in the original menu's order) to choose which enemies spawn; omit it for "Bring 'em on!".

**Controls:** W/A/S/D to move, mouse to look, **E** or **Space** to operate a door or push marked secret wall, **1–4** to select a weapon, **left mouse button** or **Left Ctrl** to fire (gamepad right shoulder/trigger also fires), Escape to quit. Locked doors require their corresponding key. The mouse is captured while the window is focused. Firing uses ammunition but does no damage yet.

## Tools

`tools/sprite_voxelizer` is a standalone offline tool, not part of the game build. It extracts all 436 WL6 sprites from `VSWAP.WL6`, removes painted floor shadows, and rebuilds every pose as a 64×64×64 MagicaVoxel model. Eight-way rotation sets are carved into one solid; camera-facing frames get depth from the actor's own body or a fitted profile. Each actor or object is written as a single `.vox` file holding all of its animations, and every model is validated against the original frames. A dependency-free HTML viewer (`tools/sprite_voxelizer/viewer`) shows the results in 3D. `--assets <dir>` installs the subject files where the game loads them. See [its README](tools/sprite_voxelizer/README.md). The output is derived from your own game data and is not committed.

`tools/audio_extractor` is a standalone offline C17 tool. It reads the VSWAP DigiList and emits named OGG files for all digitised sound effects using FFmpeg/libvorbis. It never modifies the original WL6 file. It currently does not render the remaining AdLib/PC-speaker sound chunks or music tracks; see [its README](tools/audio_extractor/README.md) for the exact scope and command line.

