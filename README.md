# WolfVK

WolfVK is a modern Windows reimagining of **Wolfenstein 3D**, written in C17. It uses SDL3 for windowing and input and Vulkan for 3D rendering. The aim is to load the original game's level and wall graphics data rather than requiring replacement assets.

Development follows the ordered steps in [roadmap.md](roadmap.md). The original [Wolf3D repository](https://github.com/elaverick/Wolf3D) is a read-only reference.

## Current development status

**Current milestone:** interactive WL6 levels with original wall textures, animated doors, secret pushwalls, voxel actors/objects and first-person voxel weapons. The application reads original-format MAPHEAD/GAMEMAPS data, expands the Carmack and RLEW compressed wall and info planes, identifies the original player-start marker and direction, generates exposed wall faces, and combines them with large floor and roof planes. The retail WL6 data contains 60 playable maps; MAPHEAD retains a 100-slot offset table.

Wall textures are loaded from `VSWAP.WL6`. The loader reads the page offsets and lengths, decodes the uncompressed 64×64 wall pages stored before the sprite-page boundary, converts their column-major palette indices to RGBA using the 256-colour Wolfenstein VGA palette, and makes them available as layers in a Vulkan texture array. The original wall-tile mapping is retained: wall type `n` selects page `(n - 1) × 2` for horizontal faces and the following page for vertical faces. Map values with the `0x40` flag are reduced to their base wall type before page selection. Original `AMBUSHTILE` value 106 is removed from the wall map after info-plane processing, while area markers 107 and above are floor. Door IDs `90–101` remain special cells, not wall textures. The mesh UV direction follows the original raycaster's hit direction. The GPU sampler uses nearest-neighbour filtering; integer scaling is the next roadmap item and is not implemented yet.

The floor and ceiling remain flat-coloured planes, now using neutral greys measured from the supplied original-game reference capture (floor RGB 113,113,113; ceiling RGB 56,56,56). Their vertex colours are specified in linear space so the sRGB swapchain reproduces those display values. Static objects and special actors remain voxel models, while guards, officers, SS soldiers and mutants are independent runtime actors. Their standing and patrol poses animate on the original 70 Hz clock; patrol actors move through the level using the original eight-way grid movement and info-plane path arrows. Guards, officers, SS soldiers and mutants now detect the player, pursue and attack through shared 70 Hz AI, with original family-specific state chains, attack timing, pain and death behaviour. Mutants use their distinctive two-shot attack and four-frame death sequence. The four first-person weapons are separately loaded as voxel models and drawn in a camera-attached Vulkan overlay. The overlay clears the world's depth values before drawing, then uses its own depth testing so the weapon always appears over the level while nearer weapon voxels correctly occlude farther ones. Original wall-plane door IDs 90–101 are instantiated as sliding doors using the original door page pairs from VSWAP. Normal, keyed and elevator door variants use the retail tile ID mapping; keys are collected from their original info-plane markers and the four locked-door key bits are enforced. Doors open at the original `tics << 10` rate, remain open for 300 game tics, reverse when obstructed and become traversable only once fully open. Door panels slide along their original orientation axis, use one two-sided plane to avoid coplanar depth fighting, and are removed from the draw list at full open. Secret pushwalls are identified by info-plane marker 98, can be activated while Use is held as in the original game, move at one tile per 128 game tics, reserve the next cell, stop when blocked and render using the source wall's original horizontal/vertical texture pages. Original ambush marker value 106 is cleared from the wall map, preventing phantom blocks from obstructing the player or pushwall path. The supplied Level 1 map has five secret markers on wall types 1, 4 and 10; their original page pairs (0/1, 6/7 and 18/19) are covered by a dedicated regression test. Ordinary wall cells remain solid and collidable.

### Voxel sprites

Every actor and static object that the original `ScanInfoPlane` would spawn is drawn as a solid voxel model built from its sprite by `tools/sprite_voxelizer`. The engine reads one MagicaVoxel file per subject (`grd.vox`, `stat_12.vox` ...) from an `assets/voxels` directory beside the WL6 files, or beside the executable. These files are derived from your own game data, so they are not in the repository; create them with the voxelizer's `--assets` option (see [Run](#run)). If the directory is missing, the game reports it and runs without actors, objects or weapons.

- **What is placed:** info-plane markers 23–71 become static objects `stat_0` … `stat_47` (71 is the second ammunition clip, which reuses `SPR_STAT_26`). Guards, officers, SS and mutants are runtime actors with independent position, direction, health, state timing and pose selection. They begin in their original standing or patrol state; dogs, bosses, ghosts, the mech suit and the dead guard remain static voxel instances.
- **Difficulty:** as in the original, the second and third tiers of enemy markers spawn only at "Bring 'em on!" and "I am Death incarnate!". `--difficulty 1-4` picks the level in menu order; the default is 3, the original menu's default.
- **Facing:** eight-way actors keep the compass direction the map gives them. Everything else was a sprite that always faced the player. A solid model needs a fixed facing, so these face along their longest straight run of open floor, which turns them away from nearby walls.
- **Scale and placement:** one voxel is one sprite pixel, so 64 voxels span a tile and a full-height model reaches the ceiling. Models stand at the centre of their tile on the floor.
- **Rendering:** each distinct pose a level uses is meshed once. Only faces between a voxel and empty space are kept, and same-coloured neighbours are merged into larger rectangles. Faces on the floor or against the ceiling can never be seen, so they are dropped. Colours come straight from the original palette, converted to linear space for the sRGB swapchain, with a fixed brightness per face direction so shapes read clearly without lighting. The meshes live in their own vertex buffer. Each placed model is one indexed draw with its own model-view-projection push constant, so the shaders are unchanged and door animation never re-copies model vertices. Runtime actors reuse these shared pose meshes and select their current pose independently; their world transforms update as they move. Level 1 places 142 objects at the default difficulty, using 31 poses and about 42,000 quads.
- **First-person weapons:** `knife.vox`, `pistol.vox`, `machinegun.vox` and `chain.vox` contain the voxeliser's `<subject>_ready` and `<subject>_atk` scene objects (for example, `knife_ready` and `knife_atk`). Each weapon's ready pose and four attack poses are loaded from those objects. The separate `player_*` names in the voxeliser manifest describe the original game's timed state sequences, not VOX scene objects. The weapon model is transformed relative to camera position, yaw and pitch, then drawn after the world models using a cleared depth buffer: nearby walls cannot hide it, while its own near-side voxels correctly occlude the far-side geometry. Missing weapon poses are reported but do not prevent the rest of the game from running.
- **Firing:** follows the original `Cmd_Fire` and `T_Attack`. Each weapon's four attack frames last six tics at 70 tics per second, and a press of fire starts one attack; holding it does not start another. The pistol fires once per press, on its muzzle-flash frame. The machine gun fires every 12 tics while fire is held, and the chaingun every 6 tics (and twice even for a single tap). Every shot uses one bullet from the player state, so the status bar counts down. When an attack ends with no bullets left the knife comes out; picking up ammunition brings the chosen gun back. The weapon cannot be changed part-way through an attack. The knife uses no ammunition. A firing kick is applied to the view model. Shots and knife strikes resolve against visible guards, officers, SS soldiers and mutants. Hits trigger family-specific pain/death states; kills award the original score values and can drop ammunition. Mutants fire on the first and third frames of their four-state shooting sequence.
- **Pickups** follow the original `GetBonus` (see [Pickups](#pickups)); a picked-up item disappears because the placed models are rebuilt from the info plane. Static objects use the original WL6 `statinfo[]` block flags for movement collision: tables, barrels, armour and other `block` entries stop the player and actors, while puddles, floor skeletons, the dead guard and pickups remain passable. Living actors reserve their destination tile, cannot overlap other actors or the player, and respect blocking map geometry.

All 60 retail WL6 levels were loaded at the hardest difficulty, which places every enemy tier, and every pose they need was available.

### Humanoid actor movement

Patrolling guards, officers, SS soldiers and mutants move at the original 512 fixed-point units per 70 Hz game tic. They use the original eight compass directions and follow WL6 `ICONARROWS` markers (90–97) when selecting their next tile. Actors reserve destination tiles, reject blocked walls and diagonal corners, respect blocking static objects and moving secret pushwalls, and request doors to open before waiting for the fully-open state. Player and actor spacing is enforced during movement. Guards, officers and SS soldiers use wall/door-aware line of sight, alert delays and original-style chase direction selection. Their different chase speeds and firing patterns are retained: guards fire once, officers use a fast single-shot sequence, and SS soldiers fire a four-shot burst with improved effective-distance accuracy. Enemy attacks damage the player and update the status bar; player hits damage enemies, trigger pain/death, award score and can drop ammunition. C5 adds mutant-specific AI: original two-shot timing, 3× chase speed, difficulty-dependent health, 700-point score, ammunition drops, and the mutant death sound.

### Spatial audio

The engine uses SDL3_mixer for positional playback. Each effect gets an independent mixer track, so concurrent weapon and world sounds do not replace one another. The listener follows the player camera, and world-space emitters are transformed into listener-relative coordinates before being passed to SDL3_mixer. Weapon fire, each pickup type, opening and closing doors, starting a secret-wall movement and failed Use actions are connected to named original WL6 effects.

Extracted sound files are loaded by their original WL6 sound names from `assets/sounds`. Run the [WL6 audio extractor](tools/audio_extractor/README.md) using your own `VSWAP.WL6`, `AUDIOHED.WL6` and `AUDIOT.WL6` files to produce OGG Vorbis assets. FFmpeg with `libvorbis` is required. The extractor renders all 87 AdLib effects through the pinned Opal OPL3 emulator, then exports the digitised samples listed in the original `DigiMap` over their matching names so the original recordings take precedence. The 27 AdLib music tracks are not yet converted; the `assets/music` folder remains reserved for them.

### Status bar

The original status bar is drawn along the bottom of the window from the game's own pictures in `VGAGRAPH.WL6`, using `VGAHEAD.WL6` for the chunk offsets and `VGADICT.WL6` for the Huffman tree. The pictures are decompressed and reordered from the original four-plane Mode X layout at startup.

- **Contents:** the bar is composed exactly as `DrawPlayScreen` and the `Draw*` routines in `WL_AGENT.C` place it: floor, score, lives, BJ's face, health, ammunition, gold and silver key slots and the current weapon, with numbers right-aligned and blank-padded as `LatchNumber` does. The face shows one of seven damage levels, chosen from health as in `DrawFace`, and glances left and right using the original `UpdateFace` timing: a fresh random 0–255 threshold is tested once per 70 Hz game tic against the accumulated face counter, independent of display refresh rate.
- **Real values:** the numbers come from a player state (`include/wolfvk/player_state.h`) modelled on the original `gamestate`. A new game starts as `NewGame` does, with 100 health, 8 bullets, 3 lives and the pistol, and the floor shown is the map within its episode. Health, ammunition, weapons, points and extra lives each go through one function that applies the original limits. Keys are held there too, so the key the player picks up is the one door locks check and the one the bar shows. Pickups change these values, and so the bar, as they are collected. Enemy attacks now damage the player through the same health and status-bar state functions; the four enemy families use the original difficulty-aware hit and damage rules.
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

**Demo:** C5 is complete. C6's automated four-family corridor/door integration and 30/144 FPS determinism tests are committed on `main`; the full Windows workflow passed all 241 Unity tests and packaged the latest demo in [Actions run 45963446803](https://github.com/elaverick/WolfVK/actions/runs/45963446803). Real-map encounter comparison and final in-game verification remain. Copy the three original WL6 files above beside the executable, run the voxelizer with `--assets assets\voxels` as the instructions describe, and then launch the game.

## Automated tests

The test harness uses [Unity](https://github.com/ThrowTheSwitch/Unity), a dedicated C unit-testing framework. The game test harness currently registers 241 Unity test cases (including the SwiftShader-backed renderer integration tests, audio spatial/event coverage, actor-state and animation tests, fixed-tic clock tests, C2 movement/collision regression tests, and two C6 mixed-family integration tests). The standalone audio-extractor harness adds 13 tests for VSWAP page reconstruction, malformed archives, original DigiMap aliases, AdLib chunk parsing and OPL synthesis. The weapon milestone adds eleven weapon controller, camera-transform and VOX-loader cases; C5 adds three mutant AI regression cases and one mutant voxel-pose preload case; C6 adds a shared-door four-family encounter test and a 30/144 FPS fixed-tic determinism test; the mouse-button tracking test adds one more to the previous 128-case suite, the status bar adds eighteen VGA decoder, player-state, status bar, layout and renderer cases, pickups add seven, audio event mappings add five, firing adds five, the actor-state foundation adds eight, and the fixed-tic simulation clock adds six:

- 3 SDL3 window and event tests
- 4 camera projection tests
- 13 player movement, mouse-look and collision tests, including original static-object blocking, door traversal and secret-wall pushing
- 11 command-line parser tests, including `--difficulty`
- 7 MagicaVoxel reader tests: named animations and keyframed poses, the stored palette, oversized models, truncated or foreign data and dangling model references, against synthetic files laid out as the voxelizer writes them
- 6 voxel mesher tests: hidden floor and ceiling faces, face merging by colour, model-space orientation and linear, shaded colour
- 8 actor and object placement tests: static markers, original WL6 static-object collision flags, standing and patrolling facings, difficulty tiers, bosses, ghosts, the dead guard and open-floor facing
- 6 voxel scene tests: one mesh per shared pose, missing files and poses, tile placement, per-instance animated actor pose selection, and all four mutant shooting/death poses
- 11 Carmack/RLEW decoder and level-loader tests
- 18 wall-geometry and player-start tests, including original wall-page selection, door-side textures and all four face UV directions
- 7 VSWAP page decoder, palette expansion and wall-page mapping tests
- 5 keyboard, mouse and gamepad input tests, including held mouse-button state
- 5 VGAGRAPH decoder tests: Huffman expansion, Mode X plane order, sparse chunks and malformed files
- 8 player-state tests: the original new-game values, health, ammunition, weapon, key and extra-life rules, plus face-glance timing
- 7 pickup tests: the statinfo bonus table, the original grab range, every item's effect, items left on the floor when not needed, weapon choice limited to weapons picked up, and reporting the exact item collected for audio
- 6 status bar tests: every number, key, face and weapon position, the face for each damage level, palette expansion and the integer-scale layout
- 18 world-state tests for doors, keys, secret pushwalls and their original timing/material behaviour
- 5 audio spatialisation tests for listener-relative direction, rotation, vertical offset and invalid inputs
- 5 audio event tests for door transitions, secret-wall starts, and original pickup-to-sound mapping
- 18 Vulkan selection and renderer tests, including the SwiftShader-backed frame submission test, the model transform, a frame that draws placed model instances, and frames that draw and re-upload the status bar
- 18 weapon tests: original 70 Hz/six-tic attack timing, automatic repeat, weapon selection, pose selection, view-model anchoring, all 20 named VOX poses and partial-asset fallback, plus firing: the shot frame, one bullet per shot, a fresh press per pistol or knife attack, machine gun and chaingun fire rates, and the switch to the knife when the bullets run out
- 8 actor-state tests: actor initialisation, exact state duration, persistent-state think cadence, pain-to-death transition, terminal-state removal, invalid definitions, immediate state changes from think callbacks and inactive-state handling
- 6 simulation-clock tests: 70 Hz tick emission, frame-partition independence, bounded catch-up, fractional-time retention, invalid elapsed-time rejection and reset
- 11 actor movement tests: all eight grid directions, original info-plane path arrows, wall and map-edge blocking, diagonal corner checks, blocking/passable statics, door opening and waiting, moving pushwall reservations, actor spacing, player clearance, deterministic tic stepping and invalid arguments

CTest runs the suite in Windows CI. The world-state tests cover original door orientation and locks, opening/closing speeds, the 300-tic timeout, obstruction handling, key collection, door-page selection, horizontal and vertical panel movement, fully-open panel removal, and secret pushwall movement/collision, valid texture-page selection, held-use activation, the Level 1 secret-wall page pairs and ambush-marker removal.

| Feature | Status |
|---|---|
| Windows x64 build | C6 code/test checkpoint passed the complete Windows build, all 241 Unity tests, both tool suites and packaged-demo workflow in [Actions run 45963446803](https://github.com/elaverick/WolfVK/actions/runs/45963446803) |
| SDL3 window and input | Implemented and tested; left mouse button state covered by a regression test |
| Vulkan renderer and swapchain | Implemented and verified by the complete Windows CI workflow in [Actions run 45963446803](https://github.com/elaverick/WolfVK/actions/runs/45963446803) |
| Indexed quad rendering and depth testing | Implemented and tested |
| Original WL6 map decoding and level selection | Implemented; 60 real WL6 maps inspected and synthetic original-format fixtures tested |
| Original VSWAP wall-page decoding | Implemented; 64×64 indexed pages converted to RGBA through the original palette |
| Original wall textures on exposed faces | Implemented; horizontal/vertical page pairing and directional UV mapping tested |
| WASD movement and mouse-look | Implemented; movement uses swept boundary resolution and wall sliding |
| Sliding doors, keys and secret pushwalls | Implemented; original WL6 tile/marker mappings and timings have regression coverage |
| Voxel actors and static objects | All four humanoid families use independent runtime actors, original 70 Hz animation poses and eight-way movement. Shared perception, pursuit and combat are implemented with family-specific state chains, damage, pain, death, score, drops and audio. C6's synthetic shared-door encounter and 30/144 FPS determinism tests pass; real WL6 encounter comparison and in-game acceptance remain |
| Spatial audio | SDL3_mixer voice pool and listener-relative spatialisation cover weapons, pickups, doors, secret walls, failed Use actions and all four enemy families, including mutant gunfire and its original death cue. Runtime playback requires the 87 named OGG effects in `assets/sounds`, generated from the original WL6 archives |
| WL6 audio extractor | Renders all 87 AdLib effects through the pinned Opal OPL3 emulator and exports VSWAP digitised samples over matching names. Thirteen standalone tests cover decoding, DigiMap names and synthesis. Uses FFmpeg/libvorbis; the 27 music tracks remain outstanding |
| Player voxel weapons | Implemented: knife, pistol, machine gun and chaingun, camera-anchored overlay, original attack timing, keyboard/mouse/gamepad firing. Shots and knife strikes resolve against all four humanoid enemy families, with damage, pain/death, score and ammunition drops |
| Sprite voxelizer | Builds and passes its Windows and Linux test suites in [Actions run 45963446803](https://github.com/elaverick/WolfVK/actions/runs/45963446803) |
| Windows demo | Latest package includes the game, voxelizer, audio extractor and shaders; packaged successfully in [Actions run 45963446803](https://github.com/elaverick/WolfVK/actions/runs/45963446803) |
| Original status bar | Implemented: decoded from VGAGRAPH, drawn at an integer scale below the view and driven by the player state; original 70 Hz random face-glance timing covered by regression tests |
| Integer texture scaling | Next milestone; not implemented for the 3D scene (the status bar already uses integer scaling) |

[![Windows build and tests](https://github.com/elaverick/WolfVK/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/elaverick/WolfVK/actions/workflows/build.yml)

C1–C5 are complete on `main`. C6's automated integration slice is committed and verified: all 241 Unity tests, Windows/Linux tool builds and tests, and the packaged Windows demo passed in [Actions run 45963446803](https://github.com/elaverick/WolfVK/actions/runs/45963446803). The shared-door test exercises all four families together, and the fixed-tic regression compares 30 FPS and 144 FPS schedules. C6 remains open only for representative retail WL6 in-game validation and direct encounter comparison; the user must run and verify the packaged Windows build.

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
build-audio\Release\wolf_audio_extractor.exe --vswap VSWAP.WL6 --audiohed AUDIOHED.WL6 --audiot AUDIOT.WL6 --output assets\sounds
```

The audio command requires `AUDIOHED.WL6` and `AUDIOT.WL6` as well as `VSWAP.WL6`, plus FFmpeg with libvorbis in `PATH`. It writes the 87 named sound effects to `assets\sounds`. The game directory then looks like this:

```
wolfvk.exe
shaders\            triangle.vert.spv, triangle.frag.spv
MAPHEAD.WL6
GAMEMAPS.WL6
VSWAP.WL6
AUDIOHED.WL6       required for AdLib effect extraction
AUDIOT.WL6         required for AdLib effect extraction
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

**Controls:** W/A/S/D to move, mouse to look, **E** or **Space** to operate a door or push marked secret wall, **1–4** to select a weapon, **left mouse button** or **Left Ctrl** to fire (gamepad right shoulder/trigger also fires), Escape to quit. Locked doors require their corresponding key. The mouse is captured while the window is focused. Player shots and knife strikes damage guards, officers, SS soldiers and mutants, trigger their pain/death sequences, award score and can drop ammunition.

## Tools

`tools/sprite_voxelizer` is a standalone offline tool, not part of the game build. It extracts all 436 WL6 sprites from `VSWAP.WL6`, removes painted floor shadows, and rebuilds every pose as a 64×64×64 MagicaVoxel model. Eight-way rotation sets are carved into one solid; camera-facing frames get depth from the actor's own body or a fitted profile. Each actor or object is written as a single `.vox` file holding all of its animations, and every model is validated against the original frames. A dependency-free HTML viewer (`tools/sprite_voxelizer/viewer`) shows the results in 3D. `--assets <dir>` installs the subject files where the game loads them. See [its README](tools/sprite_voxelizer/README.md). The output is derived from your own game data and is not committed.

`tools/audio_extractor` is a standalone offline C17 tool. It renders the 87 original AdLib sound effects through the pinned Opal OPL3 emulator and exports the digitised VSWAP DigiMap aliases over matching filenames, using FFmpeg/libvorbis. It does not modify the original WL6 files. The 27 original AdLib music tracks remain outstanding; see [its README](tools/audio_extractor/README.md) for the archive files and command line.

