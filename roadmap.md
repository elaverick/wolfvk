You are working in C, not C++.
Each time you change a file you must commit it. You do not have space in context to keep uncommitted files as you develop them, so GitHub must be your work area.
Your code must be readable. It must maintain line ending compatible with GitHub. Your function and variable names must be descriptive. You should include comments to allow a human reader to follow the intent of the code. Do not describe the process of the code, that should be apparent from the code itself, but do describe what the code is trying to do and why.
Your code must include a comprehensive test harness included and that it is run as part of build actions.
You must provide GitHub build actions that allow you to build and test the code.

You may use the main branch of the following repository as a reference point. You may only read from this repository, you may not write to it.

[x] Create a Window
[x] Ensure the window can render Vulkan 3D content in a performant manner.
[x] The window must be able to read inputs from the keyboard, mouse and a compatible game pad controller (such as an Xbox one controller).
[x] The engine must be able to render quads.
[x] There should be a floor plane and a roof plane which are infinite (you may use rendering cheats to achieve this).
[x] You must be able to read level data from the original Wolfenstein 3D data sources.
[x] A user should be able to select the level to render from the command line with a --level argument. If a --level arguement is not passed, you should render the first level.
[x] You must draw the level using untextured quads, you are not required to draw sprites, enemies, pick ups or doors at this time, you may skip rendering doors and secret doors.
[x] The user must be able to navigate the level using WASD keys and use the mouse to look.
[x] Build a demo release at this point and present to the user for confirmation that this works as expected.
[x] Once the above gate has passed, work on loading the textures from the original Wolfenstein data sources and applying them to the quads.
[x] Match the original reference floor and ceiling greys using sRGB-aware linear-space vertex colours.
[ ] Textures must support integer scaling to retain the pixel art aesthetic.
[x] Normal and locked doors must use original WL6 door tile IDs, key locks, orientation, interaction and `tics << 10` opening/closing behaviour.
[x] Doors must auto-close after the original 300-tic interval and reverse when obstructed.
[x] Secret pushwalls must use info-plane marker 98, the original cardinal push direction and 128-tic tile timing.

The user supplied MAPHEAD.WL6 and GAMEMAPS.WL6 for validation. All 60 playable maps decode as 64x64 levels and contain exactly one player-start marker on a floor cell. Level 1 (Wolf1 Map1) starts at tile (29, 57), facing east; after applying the original AREATILE (107+) rule, its wall map contains 3,082 non-floor cells and 1,014 floor cells. After omitting the 22 ordinary door cells and five secret pushwall cells, 3,055 solid wall cells remain and the mesh exposes 1,018 wall faces. The startup failure in the user's 14:23 executable was caused by that binary predating the area-floor normalization fix committed at 14:32; it treated every level-1 cell as solid. The corrected code now also uses the original info-plane player marker and direction.

The untextured Windows demo was presented to the user and explicitly approved ("Ok that works"). The original wall-texture milestone is now complete. The application reads VSWAP.WL6 page headers and uncompressed 64x64 wall pages, expands the indexed texels through the 256-colour Wolfenstein VGA palette, and uploads the pages as a nearest-sampled Vulkan texture array. Each base wall type selects the original horizontal/vertical page pair: (type - 1) * 2 and the following page. Map values carrying the 0x40 flag are reduced to their base type before mapping; the retail value 106 therefore uses pages 82 and 83. Door IDs 90–101 are excluded as door cells, and area markers 107+ are floor. Wall face UV direction is matched to the original HitVertWall/HitHorizWall orientation rules. The floor and ceiling remain flat-coloured; sprites, enemies, pickups and animated doors remain outside this milestone.

The latest Windows x64 run [43740769232](https://github.com/elaverick/WolfVK/actions/runs/43740769232) passed configuration, build, shader compilation, all 78 registered Unity tests, the SwiftShader-backed textured-renderer integration test, textured-demo packaging and artifact upload. The packaged artifact is named WolfVK-textured-demo-windows-x64 and requires the user's own MAPHEAD.WL6, GAMEMAPS.WL6 and VSWAP.WL6 files beside the executable.

The source has also had a documentation pass to meet the comment standard above: implementation functions throughout the C source now explain their purpose and the relevant design constraints, rather than narrating individual statements.

Door, key and secret-pushwall implementation:
- Wall-plane values 90–101 are instantiated as doors with the original even/odd orientation and lock mapping. VSWAP door pages are selected from the final eight wall pages, matching the original standard, locked and elevator page pairs.
- Door panels advance by 1024 position units per game tic at 70 tics per second, saturate at 0xffff, remain open for 300 tics, and close at the same rate. A closing door reverses if the player still overlaps the doorway. Map collision treats the door as solid until it reaches the fully-open state. The player can manually reverse an opening or closing door, subject to the original four key lock checks.
- The renderer retains a pair of dynamic quads per door, translating vertical and horizontal panels along their original in-plane axes. Full-open panels are removed from the dynamic draw list.
- Secret pushwalls are identified by original info-plane marker 98. A Use action starts movement at one tile per 128 tics, advances through at most two cells, reserves the next cell, and stops after one tile if the next destination is blocked. Level 1's five markers use wall types 1, 4 and 10; regression coverage verifies the corresponding original horizontal/vertical VSWAP page pairs.
- Original keys are collected from the WL6 info-plane markers. The four locked-door key bits are enforced.
- Movement collision now resolves the last safe point at a wall boundary rather than leaving a large gap when a subdivided movement step first meets a wall. Per-frame movement still resolves axes separately so wall sliding is retained.
- Regression tests cover door orientation, lock mapping, exact opening speed and open position, automatic closing delay, obstruction, manual reversal, key gating/collection, VSWAP door pages, both panel orientations and full-open rendering. Secret-wall tests cover direction, timing, blocked destinations and moving geometry.

The current test runner registers 96 Unity cases, including SDL3 input/window, camera, collision, level decoding, wall geometry, VSWAP texture mapping, world state and a SwiftShader-backed Vulkan frame submission test. Windows GitHub Actions builds and runs this suite, packages the demo and uploads the artifact. The current reference-colour and Level 1 secret-wall material changes are subject to the workflow run for their commit.

Integer texture scaling remains the only unchecked item on the original ordered roadmap. It must preserve nearest-neighbour pixel art sampling while selecting an integer physical-pixel scale for the rendered scene and letterboxing unused space; it has not been implemented.
