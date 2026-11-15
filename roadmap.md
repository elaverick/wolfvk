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
[ ] A user should be able to select the level to render from the command line with a --level argument. If a --level arguement is not passed, you should render the first level.
[ ] You must draw the level using untextured quads, you are not required to draw sprites, enemies, pick ups or doors at this time, you may skip rendering doors and secret doors.
[ ] The user must be able to navigate the level using WASD keys and use the mouse to look.
[ ] Build a demo release at this point and present to the user for confirmation that this works as expected.
[ ] Once the above gate has passed, work on loading the textures from the original Wolfenstein data sources and applying them to the quads.
[ ] Textures must support integer scaling to retain the pixel art aesthetic.
