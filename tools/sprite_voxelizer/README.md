# Wolfenstein 3D sprite voxelizer

An offline C17 tool that extracts every sprite from an original WL6 `VSWAP.WL6`, rebuilds each one as a 64×64×64 [MagicaVoxel](https://ephtracy.github.io/) `.vox` model, writes the original animations as animated `.vox` files, and validates every model it writes against the original frames.

It is a separate CMake project. The game does not link to it, and it reuses only the game's VGA palette table (`src/game_palette.c`).

## Build and run

```powershell
cmake -S tools/sprite_voxelizer -B build-tools -A x64 -DBUILD_TESTING=ON
cmake --build build-tools --config Release --parallel
ctest --test-dir build-tools -C Release --output-on-failure

build-tools\Release\wolf_sprite_voxelizer.exe --vswap "C:\path\to\VSWAP.WL6" --output tools\sprite_voxelizer\output
```

A full run takes about 30 seconds. The exit code is non-zero if any model or animation fails to write or fails validation. Run `--help` to see the options. `--only grd_s` rebuilds a single model, and the carving options described below can be adjusted.

Only retail WL6 data (436 sprite pages) is supported, because sprite names, rotation sets and animations come from the WL6 build of the original source.

**Do not commit the output.** It is derived from your own copyrighted game data, and `output/` is git-ignored.

## Output

| Path | Contents |
|---|---|
| `sprites/NNN_name.png` | All 436 original frames, extracted losslessly with transparency |
| `vox/models/<model>.vox` | 261 models: 25 eight-way rotation sets and 236 camera-facing frames |
| `vox/animations/<animation>.vox` | 88 animations, one model per keyframe, using MagicaVoxel's `_f` keyframes |
| `manifest.json` | Engine-facing index of models, sprites, depth settings, animations, tics, loops, follow-on sequences and validation scores |
| `validation/<model>.png` | A visual comparison sheet for each model |
| `validation/report.html` | All comparison sheets with their scores |
| `validation/metrics.csv` | Per-model metrics |

### Conventions

- **Scale:** one voxel per original sprite pixel. A model fills exactly one map tile (64 units), with the actor's axis at x = y = 32 and the floor at z = 0.
- **Axes:** these follow MagicaVoxel. x runs to the right in the front view, y runs away from the front viewer, and z is up. Actors face −y.
- **Rotation:** frame *k* + 1 of an eight-way set is the view from 45·*k* degrees anticlockwise (seen from above) of the facing direction. This is the convention `CalcRotate` in `WL_DRAW.C` uses.
- **Colour:** VOX colour *n* + 1 is Wolfenstein palette entry *n*, so no colour is approximated. Entry 255 has no VOX slot; WL6 sprites never use it, and validation would detect it if they did.
- **Timing:** animation tics are original 70 Hz game tics. A duration of 0 means the frame holds until the game changes state.

## How sprites become voxels

### Rotation sets (guards, SS, officers, mutants, dogs and the rocket)

The original `rotate` flags in the state tables identify the 25 eight-way sets. Each set becomes one solid:

1. **Registration.** The hand-drawn frames are not centred on a common axis; the guard's 45° pair, for example, is about 3 columns apart. Each frame is shifted sideways by up to ±3 whole columns to maximise silhouette agreement. Frame 1 stays fixed, so the actor remains centred on its original front view.
2. **Tolerant space carving.** A voxel is kept unless more than one frame's silhouette rejects it. Requiring every frame to agree lost legs and arms that only some frames draw consistently.
3. **Pruning.** Material that only some frames reject is removed wherever it would show as extra outline, provided no drawn pixel in any frame becomes uncovered.
4. **Colour.** Each drawn pixel votes for the colour of the voxel the model shows at that pixel. A per-voxel majority maximises the number of reproduced pixels, and ties go to the frame that faces the surface. Voxels that no frame can see take the colour of the nearest seen voxel.

### Camera-facing frames (objects, bosses, and pain, attack and death frames)

These frames were only ever drawn facing the camera, so depth has to be invented. Every voxel in a pixel's depth column takes that pixel's colour, so the front view reproduces the original exactly. Each sprite uses one of three depth profiles:

| Profile | Used for | Shape |
|---|---|---|
| `lathe` | barrels, pillars, lamps, plants, wells, urns, chalice, crown | each horizontal run becomes a disc |
| `inflate` | people, armour, bones, gibs, effects, weapons | a round cross-section from the outline's local thickness |
| `extrude` | keys, cross, chest, flag, signs, racks, table and chairs | constant thickness |

An actor's camera-facing frames take their body depth from that actor's rotation-set model. For example, the guard's depth-to-width ratio comes from `grd_s`, so his shooting and death frames are as thick as his standing model. Static object profiles are listed in `src/asset_catalog.c`.

### Animations

Animations are rebuilt from the original `statetype` tables in `WL_ACT2.C`, which are transcribed for WL6 in `src/wl6_tables.c`. States whose names differ only by trailing digits or an `s` suffix form one sequence (for example `s_grdchase1`, `s_grdchase1s`, `s_grdchase2` …). Each sequence is ordered by the states' `next` pointers. The manifest records loops and the sequence that follows. The player's weapon animations come from `attackinfo` in `WL_AGENT.C`.

## Validation

Validation reads every written `.vox` file back from disk, renders it orthographically from each original direction (cubes at their true projected width), and compares the render with the original frame.

- **Camera-facing models** must reproduce their frame exactly, with identical outline and identical palette index at every pixel. All 236 do.
- **Rotation sets** cannot be exact. The eight frames were drawn by hand and are not projections of a single solid. Any solid's views from opposite sides are exact mirror images, but the guard's diagonal walking frames mirror each other at only 0.56 IoU. Because Jaccard distance obeys the triangle inequality, no solid can exceed a mean IoU of the average of (1 + *m*)/2 over the four opposite pairs, where *m* is the pair's best mirror IoU. The tool computes this ceiling for each set. A set passes when:
  - its mean silhouette IoU is at least 85% of the ceiling (the margin covers resampling a 64-voxel grid at 45°);
  - no frame scores below 0.60;
  - at least half of the overlapping pixels match a colour drawn within one pixel;
  - the mean colour error is at most 40/255.

Results on the GOG retail WL6 data (all 261 models pass):

| | Range | Mean |
|---|---|---|
| Rotation-set mean silhouette IoU | 0.77–0.96 (standing poses 0.94–0.96) | 0.85 |
| Ceiling for those frames | 0.81–1.00 | 0.89 |
| Fraction of the ceiling reached | 0.87–0.99 | |
| Exact colour match (rotation sets) | 42–55% | 47% |
| Colour within one pixel (rotation sets) | 56–74% | 63% |
| Camera-facing models reproducing their frame exactly | 236 / 236 | |

Exact colour match is low for rotation sets because each surface voxel is seen by about three frames, and those frames disagree pixel-for-pixel in their dithered shading. A 64-voxel grid can also reproduce only about 77% of a 45° view's pixels, even when coloured from that view alone. Comparison sheets show the result directly. Each sheet has one column per frame with three rows: the original frame, the model rendered from the same angle, and a difference map (grey = same colour, yellow = different colour, red = missing from the model, blue = extra in the model). Two shaded three-quarter previews follow on the right.

## Known limitations

- Depth for camera-facing frames is invented. The front view is exact, but other angles are a plausible guess.
- Drop shadows drawn into objects and dogs become a thin dark slab on the floor.
- Walking and dog frames carry the inconsistencies of the original art. Their models are a best-fit solid, close to the achievable ceiling but visibly softer than the standing poses.
