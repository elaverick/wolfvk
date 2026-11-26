# Enemy AI implementation audit

**Checkpoint:** C0 — baseline audit and behavioural specification  
**Status:** Complete; no gameplay implementation is included in this checkpoint.

## Audited WolfVK integration points

The audit was performed against `main` at `9018038fa8ab1259e7c2bf31c1e38112e98d427d`.

| Area | Existing implementation | Consequence for enemy AI |
|---|---|---|
| Build and tests | Root `CMakeLists.txt` defines a strict C17-only build, SDL3, SDL3_mixer, Vulkan, Unity tests and CTest. Both game and test targets compile with warnings as errors. | New modules and tests must be registered in both relevant source lists. Keep the Windows workflow as the authoritative integration gate. |
| Level actor placement | `src/level_objects.c` maps WL6 info-plane markers to `WolfLevelObject` records. Guards, officers, SS and mutants already have family marker ranges, difficulty tiers, spawn facings and voxel animation names. | Preserve original marker and difficulty handling. Runtime actors should be created from these records rather than independently decoding the info plane. |
| Voxel data and scene | `src/voxel_scene.c` loads each subject VOX file and meshes the distinct poses required by the level. `WolfVoxelScene` stores a pose lookup and shared mesh ranges; `wolf_voxel_scene_build_instances` currently creates static tile-centred model instances. | Animated actors need per-instance pose selection and world transforms. Reuse the loaded pose meshes; do not reload or remesh a VOX file for every actor or frame. |
| World and collision | `WolfWorldState` in `include/wolfvk/game_world.h` owns the copied level, sliding doors, secret pushwall and player state. `wolf_world_state_position_overlaps_solid` currently resolves a player-sized circular footprint against map and dynamic world geometry. Static-object blocking flags are in `level_objects.c`. | Actor collision needs an actor-appropriate query, not reuse of the player-radius API unchanged. It must also account for actor occupancy, blocking statics, door state and the moving pushwall reservation. |
| Player movement and time | `wolf_player_update` in `src/player_controller.c` consumes render-frame delta and moves the camera. `main.c` currently performs one world/player/weapon update per rendered frame; it does not have a shared fixed 70 Hz simulation accumulator. | Add a deterministic simulation tick before actor logic. Keep input sampling and rendering frame-based, but do not let AI state durations or movement depend on display refresh rate. Decide explicitly how player and world updates share the fixed tick. |
| Player state and damage | `WolfPlayerState` already centralises health, ammunition, score, lives, keys and weapon state. `wolf_player_state_take_damage` clamps health at zero and reports death, but does not itself own the game-over flow. | Enemy damage should use this function and then drive a clear player-death/game-state response. Difficulty scaling from the original must be applied at the correct boundary. |
| Weapon events | `wolf_weapon_state_update` emits gun-shot and knife-attack counts. The current main loop consumes these only for audio; the weapon API explicitly defers damage. | Add hitscan resolution from these existing events. Do not make weapon animation or ammunition fire twice when the simulation is moved to fixed ticks. |
| Audio | `src/audio.c`, `src/audio_spatial.c` and `src/audio_events.c` provide positional playback and named WL6 effect loading. The main loop loads optional extracted effects. | Enemy alert, attack and death cues can use the existing positional audio layer. Missing user-extracted sounds must remain non-fatal. |
| Renderer | The renderer receives model meshes and model instances, and the current voxel scene is static. | Actor updates should publish transforms/pose selections through a bounded renderer-facing instance representation. Avoid coupling AI decisions to Vulkan code. |

### Missing capabilities confirmed

- No runtime enemy actor collection or actor update system exists yet; current humanoids are static voxel instances.
- No reusable animated actor state machine or fixed-tick simulation scheduler exists.
- No actor movement/occupancy collision, player line-of-sight query, enemy perception or pursuit implementation exists.
- Player shots are emitted as events but do not yet resolve against actors.
- There is no enemy health, pain, death, scoring/drop handling or enemy-to-player damage integration.

## Original Wolf3D source audit

The read-only reference is `elaverick/Wolf3D`, branch `master`, under `WOLFSRC/`. The following files were inspected:

- `WL_DEF.H`: actor limits, tile constants, actor flags, directions and the original actor/state data types.
- `WL_STATE.C`: actor creation and state entry, `TryWalk`, `MoveObj`, path/chase direction selection, door waiting, line-of-sight and actor damage/death support.
- `WL_ACT2.C`: difficulty-dependent starting hit points, four enemy state tables, first-sighting reactions, shared stand/path/chase/shoot routines and death/drop behaviour.
- `WL_AGENT.C`: player damage and difficulty scaling, player health/status handling and the original player weapon attack event timing.
- `WL_ACT1.C`: static-object blocking flags and world-object behaviour relevant to actor collision.

### Behavioural facts to preserve

**Simulation and state model**

The original actor system uses a linked state definition with a sprite/pose, duration in game tics, optional think routine, optional entry action and next state. `NewState` resets the actor's tic count to the new state's duration. Think routines execute as part of the actor update, while state transitions follow the 70 Hz game tic clock. Movement is grid-oriented: `TryWalk` reserves a destination tile, rejects walls and shootable actors, and can request a door to open; the actor waits until the door is fully open before completing the step. `MoveObj` moves within a tile and prevents ordinary actors overlapping the player.

**Shared behaviour**

- Standing actors run `T_Stand` and check for the player.
- Patrol actors use `T_Path`, selecting grid directions and checking sight before continuing along their patrol.
- Alerting is handled by `FirstSighting`; it changes the state sequence and increases movement speed by an enemy-specific multiplier.
- Pursuit uses `T_Chase`, `SelectChaseDir` and `SelectDodgeDir`. It is not a general-purpose shortest-path solver.
- `CheckSight` first checks connected areas, uses a close-range visibility shortcut, applies a facing-direction check and then traces the intervening map cells. `CheckLine` accounts for door opening position.
- Shooting is a timed state action. `T_Shoot` rechecks area connectivity and line of sight, then computes hit chance and damage using distance, whether the player is moving quickly and whether the player can see the attacker.
- `DamageActor` doubles damage to actors not yet in attack mode, alerts them on first damage, selects one of two pain frames based on remaining-health parity, and transitions lethal damage through `KillActor`.
- Death awards enemy-specific points, changes the actor to its death chain, removes its shootable/occupancy flags and may drop ammunition or a weapon.

**Enemy-specific configuration and state differences**

| Family | Starting hit points (Baby / Easy / Medium / Hard) | First-sighting speed multiplier | Attack sequence distinction |
|---|---:|---:|---|
| Guard | 25 / 25 / 25 / 25 | ×3 | Three 20-tic shooting states; the shot action occurs in the middle state. |
| Officer | 50 / 50 / 50 / 50 | ×5 | 6-, 20- and 10-tic shooting states; the shot action occurs in the middle state. |
| SS | 100 / 100 / 100 / 100 | ×4 | Nine shooting states; four timed shot actions are interleaved through the burst. SS hit probability receives the original distance advantage. |
| Mutant | 45 / 55 / 55 / 65 | ×3 | Four shooting states with two shot actions; the first shot occurs on entry to the first shooting state and the second in the third. |

All four families share the original four-part patrol and chase walk cycles (20, 5, 15, 20, 5 and 15 tics across the six state entries), with family-specific sprite sequences. Pain states last 10 tics. Death chains differ: guard has three 15-tic frames; officer has four 11-tic frames; SS has three 15-tic frames; mutant has four 7-tic frames. The final dead state loops indefinitely.

On first sighting, the original plays family-specific alert cues where applicable: guard `HALTSND`, officer `SPIONSND`, SS `SCHUTZADSND`; mutant transitions without one of those three cues. The original family scores are guard 100, officer 400, SS 500 and mutant 700. Guards, officers and mutants drop a used ammunition clip. SS drops a machine gun if the player does not already have one, otherwise a used clip.

The original player's damage routine quarters incoming damage on Baby difficulty before applying it. Preserve this difficulty rule when implementing the player damage path.

## Proposed implementation boundaries

1. **Actor data and animation:** introduce a runtime actor representation separate from immutable `WolfLevelObject` placement records. Give each actor a stable ID, family, tile and world position, direction, health, flags, current state ID, elapsed state tics, movement progress and per-instance data. Define states as static C data with explicit transition and action/think dispatch.
2. **Fixed simulation:** add a bounded accumulator that advances simulation in exact 1/70-second steps, caps catch-up work after stalls, and makes state timing independent of render cadence. Add deterministic tests for elapsed-time partitioning and catch-up limits.
3. **Movement and occupancy:** port the original eight-way direction model and tile reservation semantics. Add world queries for actor movement that include walls, current door aperture/state, moving pushwalls, blocking static objects and other live actors. Preserve the original door-wait rule.
4. **Perception and combat:** implement area connectivity/line checks and facing/close-range sight rules from the source, then guard alert/chase/shoot. Reuse the weapon events for player attacks, add deterministic hitscan tests, and centralise actor/player damage.
5. **Family definitions:** add officer, SS and mutant state data one family at a time, keeping shared mechanics common and mutant/SS attack sequences explicit.
6. **Lifecycle and presentation:** add pain/death, points and drops, state-specific VOX pose lookup, actor sound events and integration tests in actual decoded WL6 maps.

The runtime actor model should retain the original map marker and source `WolfLevelObject` data for stable spawning and test traceability. Avoid pointer-linked state definitions as the only identity: stable enum/index state IDs make deterministic tests, serialisation/debugging and manifest lookup easier while retaining the original state-chain semantics.

## Test plan

- State entry, exact duration, timed transitions, loops, one-shot chains, entry actions and think-call cadence.
- Fixed-tick equivalence when the same elapsed time is supplied as different render-frame partitions; long-stall catch-up cap.
- Actor cardinal/diagonal movement, tile boundaries, blocked walls, blocking statics, actors, open/closed/partially opening doors and pushwalls.
- Sight: same/adjacent tile, facing, close-range shortcut, blocked wall, open/closed door and connected/disconnected areas.
- Guard: patrol/stand alert, pursuit/dodge, attack eligibility, hitscan, player damage, being shot, pain, death, score and clip drop.
- Officer/SS/mutant: exact state chains and attack action tics, hit points, speed multipliers, damage, death, score and drops.
- Integration: multiple actors, simultaneous updates, all four difficulty levels, representative WL6 maps and stable results across render frame rates.
- Full Windows build, Unity/CTest suite, SwiftShader Vulkan integration test and standalone tool tests.

## Source references

All references below are read-only and remain in the user's original Wolf3D repository:

- [WL_DEF.H](https://github.com/elaverick/Wolf3D/blob/master/WOLFSRC/WL_DEF.H)
- [WL_STATE.C](https://github.com/elaverick/Wolf3D/blob/master/WOLFSRC/WL_STATE.C)
- [WL_ACT2.C](https://github.com/elaverick/Wolf3D/blob/master/WOLFSRC/WL_ACT2.C)
- [WL_AGENT.C](https://github.com/elaverick/Wolf3D/blob/master/WOLFSRC/WL_AGENT.C)
- [WL_ACT1.C](https://github.com/elaverick/Wolf3D/blob/master/WOLFSRC/WL_ACT1.C)

No files in Wolf3D were modified.
