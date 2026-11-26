# WolfVK humanoid enemy AI roadmap

**Scope:** First playable enemy-AI milestone: guards, officers, SS soldiers and mutants.

**Reference implementation:** The original id Software Wolfenstein 3D source in [elaverick/Wolf3D](https://github.com/elaverick/Wolf3D). This repository is a read-only reference; all implementation work belongs in WolfVK.

**Implementation language and platform:** C17, Windows, SDL3 and Vulkan. Do not introduce C++.

**Status:** C0 audit, C1 actor-animation foundation and C2 movement/collision are complete and verified by Windows CI. C3 shared perception and the guard vertical slice are next.

## Goal

Implement faithful, functional AI and movement for the four standard humanoid enemy families:

- Guards
- Officers
- SS soldiers
- Mutants

Treat these as one coherent milestone because they share the same core actor simulation and much of their decision-making. Do not implement four unrelated AI systems. Build reusable actor mechanics and a data-driven state machine, then define each enemy's distinct state sequences, attributes and attack behaviour.

The original game's design is the behavioural reference: fixed-tick state transitions, shared think/action routines, grid-oriented movement and simple line-of-sight combat. The goal is to reproduce its characteristic behaviour, not replace it with modern navigation, utility AI or behaviour trees.

## Scope boundaries

### Included

- Actor animation and state timing needed by moving and attacking enemies.
- Shared actor state-machine/update infrastructure.
- Actor movement, wall/door collision and actor-to-actor interaction where the original requires it.
- Player detection, alerting, pursuit, turning, attack positioning and line of sight.
- Guard, officer, SS and mutant-specific state sequences and combat characteristics.
- Enemy damage, pain, death, player damage and integration with the existing player/status-bar model.
- Original voxel poses and sound effects for these enemies, where available.
- Unit, integration and regression tests, plus Windows CI verification.

### Explicitly deferred

- Dogs and their bite-specific behaviour.
- Bosses and their bespoke attacks/state sequences.
- Ghosts, spectres and other special actors.
- Projectile actors and any projectile-specific AI.
- General-purpose navigation or pathfinding beyond what is needed to reproduce the original four enemy families.
- Reworking unrelated player, weapon, renderer, audio or level systems except where a small, justified integration change is required.

Do not silently expand this milestone. Record newly discovered prerequisites or scope changes here and explain why they are necessary before implementing them.

## Architectural direction

Use a shared actor simulation with data-driven state definitions.

A state should describe the relevant original animation/pose, duration in 70 Hz game tics, transition target and optional shared think/action behaviour. Shared routines should implement common operations such as standing, pathing, chasing, visibility checks, shooting, pain and death. Enemy definitions should select and sequence those routines and supply their own parameters.

Keep the distinction clear:

- **Shared mechanics:** actor lifecycle, state timing, movement, collision, perception, damage and common attack execution.
- **Enemy-specific configuration:** health, speed, reaction timing, attack damage/range, and state/animation sequences.
- **Enemy-specific behaviour:** only add dedicated C functions when the original enemy genuinely behaves differently. Mutants, for example, should not be forced into an ordinary guard firing sequence if their original state/attack logic differs.

Prefer explicit, readable C structures and function pointers or equivalent C-compatible dispatch. Avoid opaque generic frameworks and avoid duplicating the same chase or movement code in each enemy module.

Before choosing final file boundaries, inspect the current actor placement, voxel-scene, world-state, player-controller, collision, weapon and audio code. The current project places actors as static voxel models; do not assume an actor simulation already exists. Reuse existing code where it is appropriate, but do not overload unrelated modules with all enemy logic.

The original source should be consulted for exact state chains, tick durations, movement speeds, attack timing, sight checks, damage rules and difficulty-dependent behaviour. Record any deliberate deviations.

## Ordered implementation stages and checkpoints

Each checkpoint is a commit boundary. Keep the changes small enough that a checkpoint can be reviewed and recovered independently. The checkpoint must include its tests and any necessary documentation; do not leave a feature half-implemented across an extended CI wait.

### Stage 0 — Baseline audit and behavioural specification

- [x] Read the current WolfVK roadmap, CMake/build configuration, actor/object placement, voxel scene, player movement/collision, world/door state, weapon firing/damage events, player state, status bar and audio event code.
- [x] Inspect the corresponding original Wolf3D source routines and state tables for guards, officers, SS soldiers and mutants.
- [x] Identify the exact existing data and interfaces that can be reused, and list missing capabilities.
- [x] Write down per-enemy state sequences, animation names, tick durations, movement/attack parameters and difficulty rules from the reference source.
- [x] Confirm how the game advances simulation time and whether a deterministic 70 Hz actor tick can be used independently of render frame rate.
- [x] Record a concise implementation design and test plan before substantial code changes.

**Checkpoint C0 — audit complete:** [docs/enemy-ai-audit.md](enemy-ai-audit.md) records the inspected WolfVK integration points, original source behaviour, missing capabilities, proposed architecture and test plan. No gameplay implementation is included in this checkpoint. The audit is committed on `main` at `b66bfa2755bd245e7ecc63974d9e2d229f7b785d`.

**Acceptance:** The design is based on inspected WolfVK code and the original source, identifies integration points and risks, and does not assume unsupported systems exist.

### Stage 1 — Actor animation and deterministic state timing

**Progress (2026-11-26):** C1 is integrated and verified. The application creates independent runtime instances for guards, officers, SS soldiers and mutants from the existing difficulty-filtered WL6 placements. Standing actors use their standing pose; patrol actors cycle the six original patrol-state entries (20, 5, 15, 20, 5 and 15 tics) on the bounded 70 Hz clock. Each actor selects a pre-meshed shared VOX pose range and retains its own transform and timing. Static objects remain on the existing placement path. The simulation pauses when the window loses focus. The C1 Windows workflow passed in [Actions run 45891543752](https://github.com/elaverick/WolfVK/actions/runs/45891543752).

- [x] Extend the existing actor representation so an actor can hold a current animation/state, elapsed tic count and required per-instance state.
- [x] Implement deterministic state advancement at the original 70 Hz simulation rate, independent of display refresh rate.
- [x] Resolve named voxel poses/frames from the generated VOX animations, using the original sequence timing recorded by the voxelizer; handle missing poses without crashing or corrupting other actors.
- [x] Support timed transitions and state-entry actions without advancing multiple times for a single game tic.
- [x] Preserve static object placement/rendering while runtime humanoid actors select their own valid animated poses.
- [x] Add tests for state entry, duration, transition, looping, one-shot sequences, pause/focus behaviour and missing animation data.

**Checkpoint C1 — animation/state foundation:** Implementation, integration and tests are committed on `main` (test registration checkpoint `3a117caaba6b2ded99a518236c7a2a7be4962534`). The application now advances the original patrol animation at 70 Hz and renders the selected per-instance pose. The distinct 10, 3, 8, 10, 3, 8-tic chase cycle remains for the later AI stage. C1 passed the full Windows workflow in [Actions run 45891543752](https://github.com/elaverick/WolfVK/actions/runs/45891543752); C2 then began.

**Acceptance:** Met. Regression coverage verifies deterministic original voxel-pose animation at 70 Hz, frame-partition independence, inactive-window pause, missing assets and preservation of static object rendering. The complete Windows workflow passed.

### Stage 2 — Shared actor movement and collision

**Progress (2026-11-26):** C2 movement is integrated into the main game loop and advances on the existing bounded 70 Hz clock. Patrol actors use the original 512 fixed-point-units-per-tic speed, the eight original compass directions and WL6 `ICONARROWS` (90–97) for tile-to-tile path selection. Destination tiles are reserved while actors traverse them. Collision checks include walls, blocking `statinfo[]` objects, diagonal corner cells, moving pushwall reservations, the animated door state, other actors and player clearance. Actors request doors to open and wait for the fully-open state before crossing, matching `TryWalk`/`T_Path` semantics. Eleven new movement tests cover all eight directions, original path arrows, walls and map edges, diagonal corners, static objects, doors, pushwalls, actor spacing, player clearance, deterministic tic stepping and invalid inputs. The complete suite now registers 219 tests. C2 passed the full Windows build, test suite, standalone tool builds and packaged-demo workflow in [Actions run 45893803905](https://github.com/elaverick/WolfVK/actions/runs/45893803905), on commit `b60f4837c40777fcb831efa07755e49e4cb84920`.

- [x] Implement actor world position and movement in the same coordinate system used by the player and level.
- [x] Reproduce the original grid-oriented movement and direction selection required by the four enemy families, including the original info-plane path arrows.
- [x] Resolve wall, door and secret-pushwall collision consistently with current world state.
- [x] Implement actor collision/spacing rules needed to stop enemies occupying invalid or overlapping positions, guided by the original implementation.
- [x] Ensure actors cannot move through solid static objects where the original blocks them; distinguish blocking objects from passable decorations and pickups.
- [x] Make movement use elapsed game tics rather than render-frame delta.
- [x] Add tests for all cardinal and diagonal directions, path arrows, blocked movement, doors opening/closed, pushwalls, actor spacing and deterministic movement.

**Checkpoint C2 — movement/collision:** Complete and verified on `main`. The full Windows build, all 219 Unity tests, both standalone tool builds and the packaged Windows demo passed in [Actions run 45893803905](https://github.com/elaverick/WolfVK/actions/runs/45893803905). C3 is now unblocked.

**Acceptance:** Met. Controlled actors traverse valid corridors, cannot cross blocking geometry, open and wait for doors, respect player/actor/static-object/pushwall collision and behave deterministically at 70 Hz.

### Stage 3 — Shared perception and guard vertical slice

- [ ] Implement player detection using the original visibility/line-of-sight rules, including intervening walls and relevant door state.
- [ ] Implement alerting and transitions from stand/patrol into pursuit.
- [ ] Implement shared path/chase direction selection and turning based on the original rules, not modern A* navigation.
- [ ] Implement attack eligibility, line-of-sight revalidation, attack state timing and the original hitscan/attack resolution needed by guards.
- [ ] Connect enemy attacks to player damage and the existing player-state/status-bar update path.
- [ ] Implement enemy health, damage reception, pain response and death-state handling.
- [ ] Implement the guard as the first complete playable enemy, including its original standing, patrol, chase, attack, pain and death sequences.
- [ ] Connect appropriate original guard audio cues where the source and existing audio system support them.
- [ ] Add deterministic tests for sight blocked/unblocked, detection range/angle, alert transitions, pursuit direction, attack gating/timing, player damage, enemy damage, pain and death.

**Checkpoint C3 — guard vertical slice:** Commit the complete shared perception/combat foundation and working guard implementation, with tests. This is a mandatory checkpoint before adding the other three enemy families.

**Acceptance:** Guards can be spawned in a real level, animate, detect and pursue the player, attack when the original conditions are met, take damage and die. The player can be damaged, and health/status-bar values update correctly. Behaviour is stable across different render frame rates.

### Stage 4 — Officers and SS soldiers

- [ ] Add officer-specific state definitions, animation sequences and parameters using the shared mechanics.
- [ ] Reproduce officer alert, pursuit, attack cadence, damage and death behaviour from the original.
- [ ] Add SS-specific state definitions, animation sequences and parameters.
- [ ] Reproduce SS alert, pursuit, attack cadence, damage and death behaviour from the original.
- [ ] Preserve genuine differences in the original state chains and attributes; do not implement them as renamed guards.
- [ ] Add tests that prove their state sequences and combat characteristics differ where the original does, while shared movement/perception remains common.
- [ ] Verify their voxel animation names against the generated assets and report any missing source poses.

**Checkpoint C4 — officers and SS:** Commit each enemy family as a separate small change where practical; in all cases commit and test officers before beginning SS, then commit and test SS before moving to mutants.

**Acceptance:** Officers and SS soldiers work in-game with their own original animation and combat characteristics, without duplicated shared AI code or regressions to guards.

### Stage 5 — Mutants

- [ ] Add mutant-specific state definitions and animation sequences.
- [ ] Reproduce the mutant's original attack behaviour and timing; do not assume it is identical to the guard/officer/SS attack implementation.
- [ ] Reuse shared perception, pursuit, movement, collision, damage and death systems wherever the original behaviour permits.
- [ ] Add tests for mutant-specific transitions, attack timing, damage, pain and death.
- [ ] Verify mutant voxel poses and audio integration.

**Checkpoint C5 — mutants:** Commit the mutant implementation and its tests independently.

**Acceptance:** Mutants are fully playable and exhibit their original distinctive behaviour, while the other three enemy families continue to pass their tests.

### Stage 6 — Four-enemy integration and fidelity pass

- [ ] Spawn all four enemy families through the existing WL6 info-plane/difficulty rules.
- [ ] Verify all four enemy families across representative maps, including corridors, junctions, doors, ambush positions and multiple simultaneous enemies.
- [ ] Check enemy movement and combat at different render frame rates while preserving fixed-tic simulation.
- [ ] Verify damage, death, player status-bar updates, voxel animations and sound effects end to end.
- [ ] Compare representative encounters against the original game for detection, pursuit, turning, attack timing, damage and death behaviour.
- [ ] Check actor/player/world collision edge cases and ensure enemies do not become trapped or pass through walls/doors.
- [ ] Run the complete Windows build, unit suite, renderer integration tests and all existing tool tests.
- [ ] Update README and the main roadmap with the implemented scope, controls, known limitations, test counts and verified CI run.

**Checkpoint C6 — milestone complete:** Commit integration fixes, documentation and final regression coverage. Wait for and inspect the corresponding GitHub Actions run before declaring the milestone complete.

**Acceptance:** All four enemy families work together in a real WL6 level, the full CI suite passes, and the user has an opportunity to verify the Windows build in-game.

## Commit and CI checkpoint protocol

This protocol is mandatory throughout the work. It exists specifically to prevent loss of progress when GitHub Actions runs are slow, sessions are interrupted or context has to be reconstructed.

1. **Commit at every checkpoint above.** A checkpoint is a real Git commit on the authorised WolfVK branch, not merely a note that work is ready.
2. **Commit smaller logical slices between major checkpoints.** Do not accumulate an entire stage's implementation before saving it. As a practical limit, finish and commit one independently testable capability at a time (for example, state timing, then animation lookup, then movement collision).
3. **Include tests in the same commit as the behaviour they protect.** Do not defer all tests until the end of a stage.
4. **Commit before starting or waiting on a long-running workflow.** The commit must contain every source, test, build configuration and documentation change needed for that checkpoint. The working tree must not contain uncommitted implementation work when waiting for CI.
5. **Record the commit SHA and workflow run URL.** After pushing, note the exact commit and Actions run. Do not infer success from a started workflow or from a previous commit's green status.
6. **While CI runs, work only from the committed repository state.** If additional safe work is possible, make it a new, separately committed change; never rely on local uncommitted state surviving the wait.
7. **If CI fails, preserve the failed commit.** Inspect the failing job and logs, make the smallest corrective change, add or update a regression test, and commit the fix separately. Do not rewrite or discard the checkpoint.
8. **If a session ends or times out, resume from GitHub.** Inspect the branch head, recent commits and relevant workflow run before continuing. The latest pushed commit is the source of truth.
9. **Do not start a broad refactor while a checkpoint is unverified.** Prefer resolving the current failure first, unless the next change is isolated and does not obscure diagnosis.
10. **Never claim a checkpoint or stage is complete until its acceptance criteria and required CI evidence have been checked.** Distinguish code committed, local tests passed, CI passed and in-game behaviour verified.

A commit message should identify the capability and stage, for example:

- `Add deterministic actor state timing (C1)`
- `Implement actor wall collision (C2)`
- `Add guard perception and pursuit (C3)`
- `Implement officer state sequences (C4)`

## Definition of done

The milestone is done only when:

- Guards, officers, SS soldiers and mutants each implement their original state/animation and combat behaviour.
- Shared mechanics are genuinely shared, with enemy-specific behaviour kept explicit and readable.
- Actor simulation is deterministic at the original 70 Hz timing.
- Enemies interact correctly with the player, walls, doors and other relevant world actors.
- All four enemy types are spawned from original WL6 level data and work in representative real levels.
- Tests cover shared systems and enemy-specific differences.
- The full Windows CI workflow passes on the final milestone commit.
- The final Windows build has been run in-game and the user has had the opportunity to verify it.

## Reference and fidelity notes

Use the original Wolf3D source as the authority for behaviour and timing, not recollection or assumptions. Relevant original source areas include the actor state/update code, enemy action/think routines, actor and state declarations, level actor spawning, player damage and weapon hit resolution. Verify exact source filenames and function names during Stage 0.

Do not copy original DOS-era implementation wholesale where it conflicts with WolfVK's C17 architecture. Port the behaviour and timing, retain clear module boundaries, and document any deliberate deviations.
