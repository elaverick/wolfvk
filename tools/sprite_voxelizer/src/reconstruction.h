#ifndef WOLF_SPRITE_VOXELIZER_RECONSTRUCTION_H
#define WOLF_SPRITE_VOXELIZER_RECONSTRUCTION_H

#include "sprite_image.h"
#include "voxel_model.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Rotation frames fall into two groups. The four face-on frames (front,
 * sides, back) are drawn consistently with one another, while the four
 * diagonal frames were drawn independently and often show a different
 * stride or arm swing. The face-on frames therefore define the solid, and
 * the diagonal frames are only used to resolve what four orthogonal
 * silhouettes cannot (which leg is forward, which arm is raised).
 */
typedef struct WolfMultiViewOptions {
    /*
     * Largest sideways shift, in canvas columns, tried when registering a
     * frame to the shared vertical axis. The artists did not centre every
     * rotation on the same column; 0 disables registration.
     */
    int maximum_registration_shift;
    /* Connected fragments smaller than this many voxels are discarded as carving noise. */
    size_t minimum_fragment_voxels;
    /*
     * Remove material two or more frames reject, provided every drawn pixel of the
     * face-on frames stays covered. This removes the phantom limbs a
     * four-view hull invents (a walking figure otherwise grows four legs)
     * and the excess left where opposite frames disagree.
     */
    bool prune_excess_voxels;
    /*
     * Every voxel should touch at least this many occupied face neighbours
     * (the floor counts). Weaker voxels (flakes, spurs, one-voxel strands) are
     * removed repeatedly, except where one is the only voxel drawing part of
     * a face-on frame. 0 disables the rule.
     */
    int minimum_face_neighbours;
} WolfMultiViewOptions;

WolfMultiViewOptions wolf_multi_view_default_options(void);

/* Face-on views (multiples of 90 degrees) define the solid; others refine it. */
bool wolf_rotation_is_face_on(double rotation_degrees);

/*
 * Rebuild one solid from several rotation frames:
 *  1. register the face-on frames to a common axis and carve their visual
 *     hull, taking the union of each opposite pair;
 *  2. register each diagonal frame against that hull;
 *  3. prune hull material two or more frames reject without uncovering any
 *     face-on drawn pixel, then restore any face-on drawn pixel the
 *     intersection lost with a voxel next to the body;
 *  4. round the head's vertical edges, remove weakly attached voxels and
 *     join any remaining fragment to the body, so the solid is one piece;
 *  5. colour each surface voxel from the frame that looks most squarely at
 *     it, so every face keeps the crisp texture of the frame drawn for it.
 * Voxels no frame can see inherit the colour of the nearest seen voxel.
 *
 * registration_shifts (optional, view_count entries) receives the column
 * offset of each frame; the first face-on frame is the fixed reference.
 */
bool wolf_reconstruct_multi_view(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    size_t view_count,
    const WolfMultiViewOptions *options,
    WolfVoxelModel *model,
    int *registration_shifts);

/* How depth is invented for a sprite that was only ever drawn from one direction. */
typedef enum WolfDepthProfile {
    /* Round every limb and blob by its own width, like inflating the outline. */
    WOLF_DEPTH_PROFILE_INFLATE,
    /* Turn each horizontal run into a disc, suiting barrels, lamps and pillars. */
    WOLF_DEPTH_PROFILE_LATHE,
    /* A constant thickness, suiting signs, flat treasure and floor clutter. */
    WOLF_DEPTH_PROFILE_EXTRUDE,
    /*
     * A first-person weapon seen from behind the player's eye. Perspective
     * draws the far end of the gun higher on the screen, so each row is
     * placed further from the viewer than the row below: the hand stays
     * near the eye and the barrel reaches forward into the screen. The
     * front view is unchanged because every pixel's voxels stay on its own
     * line of sight.
     */
    WOLF_DEPTH_PROFILE_FIRST_PERSON,
    /*
     * A cage of vertical bars. Pixels in the frame colours are bars and go
     * on a cylinder around the outside (front and back), solid rows become
     * hoops with the top and bottom of the cage capped, and anything else
     * (a prisoner) is inflated inside, so the bars surround the contents
     * instead of running through the middle.
     */
    WOLF_DEPTH_PROFILE_CAGE
} WolfDepthProfile;

typedef struct WolfSingleViewOptions {
    WolfDepthProfile profile;
    /* Multiplier from the profile's natural half-depth to voxels. */
    double depth_scale;
    /* Upper bound on half-depth in voxels, keeping flat items flat. */
    double maximum_half_depth;
    /* Half-depth used by the extrude profile. */
    double extrude_half_depth;
    /*
     * Cage profile only: 256 flags marking the palette entries of the bars.
     * NULL for every other profile.
     */
    const uint8_t *frame_colours;
    /*
     * For actors drawn only from the front: the back half of the body is
     * coloured plainly (hair or bare head above, the dominant colour of each
     * body region below) so the model has one face, not a mirrored second
     * one on the back of its head.
     */
    bool plain_back;
} WolfSingleViewOptions;

/*
 * Build a model from one camera-facing frame. Every voxel in a pixel's depth
 * column takes that pixel's colour, so the front view reproduces the sprite
 * exactly while the invented depth gives the object volume from other angles.
 */
void wolf_reconstruct_single_view(
    const WolfSpriteImage *image,
    const WolfSingleViewOptions *options,
    WolfVoxelModel *model);

/*
 * Build a camera-facing actor frame (pain, attack) on the body of the same
 * actor's rotation-set model. Where the frame overlaps the reference body
 * seen from the front, the voxels span the reference body's real depth: the
 * front face shows the frame and the sides and back keep the reference
 * body's own colours, so the actor is a person rather than a cut-out.
 * Pixels the reference does not cover (a raised arm, a levelled gun) fall
 * back to the given profile around the body's centre line. The front view
 * still reproduces the frame exactly.
 */
void wolf_reconstruct_pose_transfer(
    const WolfSpriteImage *image,
    const WolfVoxelModel *reference,
    const WolfSingleViewOptions *fallback,
    WolfVoxelModel *model);

/*
 * Build a projectile as a body of revolution about its flight axis, from
 * its side frame (rotation 2, nose toward screen-left). The original
 * nose-on frames are drawn wider than tall, so carving all eight frames
 * flattens the rocket; revolving the side profile gives a round body that
 * reproduces both side frames.
 */
void wolf_reconstruct_axial_revolve(const WolfSpriteImage *side_view, WolfVoxelModel *model);

/*
 * Compute each pixel's half-depth for a single-view profile. Exposed for
 * testing; transparent pixels receive zero. The first-person profile uses
 * the inflate half-depth here; its forward placement is applied when the
 * model is built.
 */
void wolf_single_view_half_depths(
    const WolfSpriteImage *image,
    const WolfSingleViewOptions *options,
    double *half_depth);

/*
 * Ratio of a model's front-to-back thickness to its side-to-side width,
 * measured layer by layer. Used to give single-view frames of an actor the
 * same body depth as that actor's multi-view reconstruction.
 */
double wolf_voxel_model_depth_ratio(const WolfVoxelModel *model);

/* Front-to-back extent of the occupied voxels, in voxels (0 for an empty model). */
int wolf_voxel_model_depth_extent(const WolfVoxelModel *model);

#endif
