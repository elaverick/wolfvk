# Wolfenstein 3D sprite voxelizer

An offline C17 tool that extracts every sprite from an original WL6 `VSWAP.WL6`, removes the painted floor shadows, rebuilds each pose as a 64×64×64 [MagicaVoxel](https://ephtracy.github.io/) model, and writes **one `.vox` file per actor or object** containing all of its animations. Every model is checked against the original frames, and a small HTML viewer is included for inspecting the results in 3D.

The tool is a separate CMake project: the game neither builds nor links it. The only game code it reuses is the VGA palette table (`src/game_palette.c`).

## Build and run

```powershell
cmake -S tools/sprite_voxelizer -B build-tools -A x64 -DBUILD_TESTING=ON
cmake --build build-tools --config Release --parallel
ctest --test-dir build-tools -C Release --output-on-failure

build-tools\Release\wolf_sprite_voxelizer.exe --vswap "C:\path\to\VSWAP.WL6" --output tools\sprite_voxelizer\output
```

A full run takes about 25 seconds. The exit code is non-zero if any file fails to write or any pose fails validation. Use `--only grd` to rebuild a single subject; `--help` lists the remaining options.

Only retail WL6 data is supported (436 sprite pages), because the sprite names, rotation sets and animations come from the WL6 build of the original source.

**Do not commit the output.** It is derived from your own copyrighted game data, and `output/` is git-ignored.

## Viewer

Open `tools/sprite_voxelizer/viewer/index.html` in a browser after a run. It loads `../output/viewer_data.js` automatically. If your output is somewhere else, the page asks you to choose that file.

- Pick a subject on the left, then an animation and pose at the bottom. **Play** cycles the poses at 6 fps, or follows one of the game's original state sequences at the original 70 Hz tic timing (for example, `grdchase` holds each stride for its own number of tics).
- Drag to orbit, right-drag (or shift-drag) to pan, scroll to zoom, double-click to reset.
- Buttons **1–8** (or keys 1–8) snap the camera to that original rotation, in orthographic projection at eye level. The original frame and the scores for that rotation appear on the right.
- Deep links open a pose directly, for example `index.html#subject=dog&animation=1&pose=2&yaw=90` or `#subject=grd&rotation=2`.

The viewer is plain WebGL2 with no dependencies. `viewer_data.js` embeds the manifest, every `.vox` file and every original frame, because browsers block pages opened from disk from reading neighbouring files.

## Output

| Path | Contents |
|---|---|
| `vox/<subject>.vox` | 77 files: one per actor (`grd`, `dog`, `ss`, `mut`, `ofc`), boss, projectile, effect, weapon (`knife`, `pistol`, `machinegun`, `chain`) and static object (`stat_0` … `stat_47`) |
| `manifest.json` | Engine-facing index of subjects, animations, poses, source sprites, reconstruction method, validation scores, and the original timed state sequences mapped onto those poses |
| `sprites/NNN_name.png` | All 436 original frames, extracted losslessly with transparency |
| `validation/<model>.png`, `report.html`, `metrics.csv` | Visual comparison sheets and per-pose metrics |
| `viewer_data.js` | Everything the viewer needs, in one script |

### Inside a subject file

Each animation is a named object in the MagicaVoxel scene. Its poses are the object's keyframes (`_f` = 0, 1, 2, …), so MagicaVoxel plays the animation and an engine can address it by name. For example, `grd.vox` contains:

| Object | Poses | Source sprites |
|---|---|---|
| `grd_s` | 1 | `SPR_GRD_S_1…8` (eight-way set, rebuilt into one model) |
| `grd_w` | 4 | `SPR_GRD_W1_1…8` … `SPR_GRD_W4_1…8` |
| `grd_pain` | 2 | `SPR_GRD_PAIN_1`, `SPR_GRD_PAIN_2` |
| `grd_die` | 3 | `SPR_GRD_DIE_1…3` |
| `grd_dead` | 1 | `SPR_GRD_DEAD` |
| `grd_shoot` | 3 | `SPR_GRD_SHOOT1…3` |

Objects are spaced 80 voxels apart along x so all of them are visible at once in MagicaVoxel. The manifest's `sequences` section gives the original timing. For example, `grdchase` plays `grd_w` poses 0,0,1,2,2,3 for 10,3,8,10,3,8 tics.

### Conventions

- **Scale:** one voxel per sprite pixel. A model fills exactly one map tile (64 units), with the actor's axis at x = y = 32 and the floor at z = 0.
- **Axes:** these follow MagicaVoxel. x runs to the right in the front view, y runs away from the front viewer, and z is up. Actors face −y.
- **Rotation:** original frame *k* + 1 is the view from 45·*k* degrees anticlockwise (seen from above) of the facing direction. This is `CalcRotate` in `WL_DRAW.C`.
- **First-person weapons:** these extend along +y, away from the player's eye. The hand is nearest the eye and the muzzle is furthest.
- **Colour:** VOX colour *n* + 1 is Wolfenstein palette entry *n*, so no colour is approximated.

## How sprites become voxels

### Floor shadows are removed first

Objects, actors and bosses have a drop shadow painted on the floor. It is drawn in black, the darkest greys (palette 26–31) and three dedicated shadow greys (234–236). Objects use those colours too, so a pixel counts as shadow only if both of these hold:

- Scanning up its column from the floor, an unbroken run of shadow colours ends within the bottom eight rows. A dark run that continues up into the object is the object's own shading.
- The patch it belongs to is at least as wide as it is tall, because a shadow lies flat. A dark robe hem between two legs stands up.

Tiny shadow-coloured crumbs left beside a removed shadow go with it. Effects, projectiles, the title lettering and the weapons are never touched. Comparison sheets show removed pixels in purple.

### Eight-way rotation sets (guards, SS, officers, mutants, dogs, rocket)

The original `rotate` flags identify the 25 sets. The four face-on frames (front, both sides, back) are broadly consistent with each other. The four diagonal frames were drawn independently: for a walk cycle, opposite diagonals mirror each other at only about 0.56 IoU, which no solid can reconcile. So:

1. **Register** the face-on frames to a common axis by shifting them sideways by whole columns. The artists did not centre every frame on the same column.
2. **Carve** the hull of the face-on frames. Each opposite pair is combined as a union (front *or* back draws it), so neither frame's small disagreements cut material away. The two viewing axes are then intersected.
3. **Register** each diagonal frame against that hull.
4. **Prune** material that any frame rejects, most-rejected and outermost first, provided every drawn pixel of every face-on frame stays covered. This removes the phantom limbs that four orthogonal silhouettes invent (a walking figure would otherwise have four legs). The diagonal frames decide which ambiguous material survives.
5. **Repair** face-on pixels the axis intersection still lost, such as a stride foot drawn at a height the front frame doesn't show. The fix is three voxels deep and placed next to the body; any repair left disconnected from the body is dropped.
6. **Colour** each voxel from the face-on frame that looks most squarely at it. Diagonal frames only colour what no face-on frame shows. Outline pixels lose to a frame that sees the voxel inside its silhouette, so drawn outlines don't become dark blotches.

### Camera-facing frames

| Method | Used for | How depth is made |
|---|---|---|
| `pose_transfer` | pain and attack frames of guards, SS, officers, mutants | The frame is built on the actor's own rotation-set body. The front face shows the frame; the body's real depth, sides and back stay underneath. Parts the body lacks (a levelled gun, a raised arm) are inflated around its centre line. |
| `inflate` | death frames, bosses, BJ, ghosts, effects, armour, bones | Rounded by the outline's local thickness. Death frames use the actor's body depth ratio; bosses use 0.8. |
| `lathe` | barrels, pillars, lamps, plants, wells, urns, chalice, crown | Each horizontal run becomes a disc. |
| `extrude` | keys, cross, chest, flag, signs, racks | Constant thickness. |
| `first_person` | the 20 first-person weapon frames | Each row higher up the screen sits further from the eye, so the barrel reaches into the screen. Thickness across the gun matches the sprite's width. |
| `hand_built` | table and chairs (`stat_2`), bed (`stat_22`) | Modelled in voxels using the sprite only as a reference for proportions and palette entries: a round pedestal table with candles and two ladder-back chairs, and a metal-framed bed with mattress and pillow. |

In every method except `hand_built`, each voxel in a pixel's line of sight takes that pixel's colour, so the front view reproduces the shadow-free original exactly.

## Validation

Every pose is rendered orthographically from each original direction (cubes at their true projected width) and compared with the shadow-free original frame. Every subject file is also read back from disk and checked object by object, keyframe by keyframe and voxel by voxel.

| Method | Pass condition |
|---|---|
| single-view methods, `pose_transfer` | Front view identical to the shadow-free original (outline and every palette index) |
| `first_person` | as above, and the weapon reaches at least 24 voxels into the screen |
| `rotation_set` | Each face-on view: silhouette IoU ≥ 0.80, or 95% of the best IoU that view and its opposite allow any solid*. Each diagonal view: IoU ≥ 0.55. Face-on colour within one pixel ≥ 60%. |
| `hand_built` | Front-view IoU against the reference ≥ 0.35. The reference furniture is drawn in three-quarter perspective, so this only catches a wrong or misplaced model. |

\* Opposite views of any solid are exact mirror images. By the triangle inequality on Jaccard distance, two opposite views whose drawn outlines mirror each other at IoU *m* can together reach a mean IoU of at most (1 + *m*)/2. The manifest records this `pair_ceiling` for every view.

### Results on the GOG retail WL6 data

- 261 poses in 77 subject files. All files were written and read back identically.
- 259 of 261 poses pass. All 236 camera-facing poses reproduce their frames exactly, and the table and bed pass their reference check.
- Rotation sets:
  - Standing poses reach a mean silhouette IoU of 0.93–0.95 (face-on views 0.92–0.99, diagonals 0.87–0.96).
  - Walking poses reach 0.75–0.86. Their face-on views score 0.78–0.97, with 77 of 80 at or above 0.80. Their diagonal frames contradict the face-on ones and score 0.64–0.87.
  - Face-on colour within one pixel is 69–81%.
- **Two poses fail**, both by a small margin:
  - `dog_w3`: side view 0.777, back view 0.787.
  - `mut_w1`: side view 0.797.
  These are left failing rather than hidden behind a looser threshold. The comparison sheets in `validation/` show where.

Colour agreement is limited by geometry. With one voxel per pixel, a voxel on the edge of a narrow figure is seen by both the front and the side frame, which drew it differently, so it can match only one of them.

## Known limitations

- Depth for camera-facing frames (bosses, death frames, objects) is invented. The front view is exact, but other angles are a plausible guess.
- Walking and dog models are a single solid fitted to frames that disagree with each other. They are clean from every side, but stride details differ from the diagonal drawings, and repaired limbs (such as a stride foot that only the side frame draws) can look blocky.
- The first-person weapon models reproduce the sprite from the player's eye and extend into the screen. Seen from the side, they are a smooth wedge rather than a modelled gun.
