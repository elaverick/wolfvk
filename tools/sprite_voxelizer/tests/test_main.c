#include "unity.h"

void test_sprite_page_round_trips_original_post_layout(void);
void test_sprite_page_decodes_hand_built_compshape(void);
void test_sprite_page_rejects_malformed_pages(void);
void test_sprite_archive_reads_only_the_sprite_range(void);
void test_sprite_archive_rejects_invalid_boundaries_and_ranges(void);

void test_view_basis_matches_original_rotation_convention(void);
void test_front_render_maps_voxels_one_to_one(void);
void test_side_and_back_renders_follow_view_direction(void);
void test_render_keeps_the_nearest_voxel(void);
void test_diagonal_render_has_no_gaps_and_wider_footprint(void);
void test_surface_detection_and_counting(void);
void test_preview_render_draws_shaded_model(void);

void test_extrude_profile_reproduces_front_view_with_constant_depth(void);
void test_lathe_profile_turns_each_run_into_a_disc(void);
void test_inflate_profile_rounds_by_local_thickness(void);
void test_single_view_never_drops_a_pixel(void);
void test_multi_view_rebuilds_a_consistent_solid(void);
void test_multi_view_registers_an_off_centre_frame(void);
void test_tolerant_carving_survives_one_inconsistent_frame(void);
void test_multi_view_rejects_invalid_arguments(void);
void test_pose_transfer_builds_on_the_reference_body(void);
void test_first_person_profile_recedes_into_the_screen(void);
void test_cage_profile_puts_bars_around_the_contents(void);
void test_cage_prisoner_continues_behind_bars(void);
void test_multi_view_result_is_one_solid_piece(void);
void test_plain_back_gives_a_single_face(void);
void test_axial_revolve_makes_a_round_body(void);
void test_lathe_wraps_the_texture_round_the_disc(void);
void test_lathe_hollow_top_opens_the_object(void);
void test_lathe_rough_surface_breaks_up_the_outline(void);
void test_solid_back_is_one_colour(void);

void test_shadow_removal_lifts_a_flat_floor_shadow(void);
void test_shadow_removal_keeps_dark_parts_of_the_object(void);
void test_shadow_removal_clears_detached_crumbs(void);
void test_shadow_palette_matches_the_painted_shadow_colours(void);
void test_light_pool_removal_keeps_the_lamp(void);
void test_shadow_fringe_trim_removes_the_dark_edge(void);

void test_table_and_chairs_model_is_furniture_shaped(void);
void test_bed_model_is_long_and_raised(void);
void test_food_model_is_a_drumstick_on_a_plate(void);
void test_first_aid_model_is_a_flat_box_with_a_red_cross(void);
void test_gibs_model_lays_blood_flat_under_the_skull(void);
void test_well_model_is_a_hollow_ring_of_constant_radius(void);
void test_well_model_holds_the_sprite_water(void);
void test_puddle_model_is_one_voxel_thick(void);
void test_bone_pile_model_is_a_low_pile_with_a_skull(void);
void test_treasure_chest_model_is_open_and_full(void);
void test_flag_model_pleats_the_cloth_round_the_pole(void);

void test_vox_single_object_round_trips_with_original_palette(void);
void test_vox_named_animations_record_keyframes(void);
void test_vox_colour_mapping_handles_the_last_palette_entry(void);
void test_vox_rejects_malformed_files_and_arguments(void);

void test_png_checksums_match_reference_values(void);
void test_zlib_stream_round_trips(void);
void test_png_encoder_writes_valid_rgba_image(void);

void test_wl6_tables_match_original_enumeration(void);
void test_catalog_groups_rotation_sets_from_state_flags(void);
void test_catalog_groups_poses_into_subject_animations(void);
void test_catalog_assigns_reconstruction_methods(void);
void test_animation_catalog_rebuilds_original_sequences(void);

void test_compare_views_scores_identical_and_disjoint_images(void);
void test_compare_views_separates_exact_and_nearby_colour(void);
void test_rotation_ceiling_reflects_opposite_view_agreement(void);
void test_comparison_sheet_lays_out_views_and_previews(void);

void test_pipeline_writes_and_validates_every_output(void);
void test_pipeline_builds_a_single_subject(void);
void test_pipeline_installs_subject_files_for_the_game(void);
void test_pipeline_refuses_non_wl6_sprite_counts(void);

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_sprite_page_round_trips_original_post_layout);
    RUN_TEST(test_sprite_page_decodes_hand_built_compshape);
    RUN_TEST(test_sprite_page_rejects_malformed_pages);
    RUN_TEST(test_sprite_archive_reads_only_the_sprite_range);
    RUN_TEST(test_sprite_archive_rejects_invalid_boundaries_and_ranges);

    RUN_TEST(test_view_basis_matches_original_rotation_convention);
    RUN_TEST(test_front_render_maps_voxels_one_to_one);
    RUN_TEST(test_side_and_back_renders_follow_view_direction);
    RUN_TEST(test_render_keeps_the_nearest_voxel);
    RUN_TEST(test_diagonal_render_has_no_gaps_and_wider_footprint);
    RUN_TEST(test_surface_detection_and_counting);
    RUN_TEST(test_preview_render_draws_shaded_model);

    RUN_TEST(test_extrude_profile_reproduces_front_view_with_constant_depth);
    RUN_TEST(test_lathe_profile_turns_each_run_into_a_disc);
    RUN_TEST(test_inflate_profile_rounds_by_local_thickness);
    RUN_TEST(test_single_view_never_drops_a_pixel);
    RUN_TEST(test_multi_view_rebuilds_a_consistent_solid);
    RUN_TEST(test_multi_view_registers_an_off_centre_frame);
    RUN_TEST(test_tolerant_carving_survives_one_inconsistent_frame);
    RUN_TEST(test_multi_view_rejects_invalid_arguments);
    RUN_TEST(test_pose_transfer_builds_on_the_reference_body);
    RUN_TEST(test_first_person_profile_recedes_into_the_screen);
    RUN_TEST(test_cage_profile_puts_bars_around_the_contents);
    RUN_TEST(test_cage_prisoner_continues_behind_bars);
    RUN_TEST(test_multi_view_result_is_one_solid_piece);
    RUN_TEST(test_plain_back_gives_a_single_face);
    RUN_TEST(test_axial_revolve_makes_a_round_body);
    RUN_TEST(test_lathe_wraps_the_texture_round_the_disc);
    RUN_TEST(test_lathe_hollow_top_opens_the_object);
    RUN_TEST(test_lathe_rough_surface_breaks_up_the_outline);
    RUN_TEST(test_solid_back_is_one_colour);

    RUN_TEST(test_shadow_removal_lifts_a_flat_floor_shadow);
    RUN_TEST(test_shadow_removal_keeps_dark_parts_of_the_object);
    RUN_TEST(test_shadow_removal_clears_detached_crumbs);
    RUN_TEST(test_shadow_palette_matches_the_painted_shadow_colours);
    RUN_TEST(test_light_pool_removal_keeps_the_lamp);
    RUN_TEST(test_shadow_fringe_trim_removes_the_dark_edge);

    RUN_TEST(test_table_and_chairs_model_is_furniture_shaped);
    RUN_TEST(test_bed_model_is_long_and_raised);
    RUN_TEST(test_food_model_is_a_drumstick_on_a_plate);
    RUN_TEST(test_first_aid_model_is_a_flat_box_with_a_red_cross);
    RUN_TEST(test_gibs_model_lays_blood_flat_under_the_skull);
    RUN_TEST(test_well_model_is_a_hollow_ring_of_constant_radius);
    RUN_TEST(test_well_model_holds_the_sprite_water);
    RUN_TEST(test_puddle_model_is_one_voxel_thick);
    RUN_TEST(test_bone_pile_model_is_a_low_pile_with_a_skull);
    RUN_TEST(test_treasure_chest_model_is_open_and_full);
    RUN_TEST(test_flag_model_pleats_the_cloth_round_the_pole);

    RUN_TEST(test_vox_single_object_round_trips_with_original_palette);
    RUN_TEST(test_vox_named_animations_record_keyframes);
    RUN_TEST(test_vox_colour_mapping_handles_the_last_palette_entry);
    RUN_TEST(test_vox_rejects_malformed_files_and_arguments);

    RUN_TEST(test_png_checksums_match_reference_values);
    RUN_TEST(test_zlib_stream_round_trips);
    RUN_TEST(test_png_encoder_writes_valid_rgba_image);

    RUN_TEST(test_wl6_tables_match_original_enumeration);
    RUN_TEST(test_catalog_groups_rotation_sets_from_state_flags);
    RUN_TEST(test_catalog_groups_poses_into_subject_animations);
    RUN_TEST(test_catalog_assigns_reconstruction_methods);
    RUN_TEST(test_animation_catalog_rebuilds_original_sequences);

    RUN_TEST(test_compare_views_scores_identical_and_disjoint_images);
    RUN_TEST(test_compare_views_separates_exact_and_nearby_colour);
    RUN_TEST(test_rotation_ceiling_reflects_opposite_view_agreement);
    RUN_TEST(test_comparison_sheet_lays_out_views_and_previews);

    RUN_TEST(test_pipeline_writes_and_validates_every_output);
    RUN_TEST(test_pipeline_builds_a_single_subject);
    RUN_TEST(test_pipeline_installs_subject_files_for_the_game);
    RUN_TEST(test_pipeline_refuses_non_wl6_sprite_counts);

    return UNITY_END();
}
