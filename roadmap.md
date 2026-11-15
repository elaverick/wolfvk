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
[ ] Once the above gate has passed, work on loading the textures from the original Wolfenstein data sources and applying them to the quads.
[ ] Textures must support integer scaling to retain the pixel art aesthetic.

The user supplied MAPHEAD.WL6 and GAMEMAPS.WL6 for validation. All 60 playable maps decode as 64x64 levels and contain exactly one player-start marker on a floor cell. Level 1 (Wolf1 Map1) starts at tile (29, 57), facing east; after applying the original AREATILE (107+) rule, its wall map contains 3,082 solid cells and 1,014 floor cells, producing 1,016 exposed wall faces. The startup failure in the user's 14:23 executable was caused by that binary predating the area-floor normalization fix committed at 14:32; it treated every level-1 cell as solid. The corrected code now also uses the original info-plane player marker and direction.

The pre-texture Windows x64 demo has been built and packaged. The Windows CI build passed and all 61 Unity tests passed on commit 9e76584. The demo is presented for user visual confirmation; visual approval remains pending. Do not start original texture loading or integer scaling until the user explicitly approves the untextured demo.
