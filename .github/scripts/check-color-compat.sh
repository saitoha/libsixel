#!/bin/sh
# Run from the current source root with the r2 checkout, a build directory
# and optionally the original v1.8.7 checkout. CI supplies both releases.
set -eu

if test "$#" -lt 2 || test "$#" -gt 3; then
    echo "Usage: $0 R2_SOURCE BUILD_DIR [V1_8_7_SOURCE]" >&2
    exit 2
fi
baseline=$(cd "$1" && pwd -P)
original=
if test "$#" -eq 3; then
    original=$(cd "$3" && pwd -P)
fi
current=$(pwd -P)
export PATH="$current/.local/bin:$PATH"
mkdir -p "$2"
build=$(cd "$2" && pwd -P)
jobs=${JOBS:-$(nproc)}

# Only the release header and shared library are needed for the old client.
# Use the pre-C23 C dialect expected by the release even on recent GCC.
check_release()
(
    mkdir -p "$2"
    cd "$2"
    "$1/configure" --disable-static --without-jpeg --without-png \
        CFLAGS="-O2 -std=gnu11"
    make -j"$jobs" -C src
    sh "$current/tests/compat/v1.8.7-r2/check.sh" \
        "$2/include/sixel.h" "$2/src/.libs/libsixel.so" \
        "$build/current/src/.libs/libsixel.so" "$build/current/include/sixel.h" \
        "$3"
)

mkdir -p "$build/current"
cd "$build/current"
"$current/configure" --enable-shared --without-lcms2 --without-coregraphics \
    --with-png --with-tiff --with-webp
grep -qx '#define HAVE_LIBPNG 1' config.h
grep -qx '#define HAVE_LIBTIFF 1' config.h
grep -qx '#define HAVE_WEBP 1' config.h
if grep -Eq '^#define HAVE_(LCMS2|COREGRAPHICS) 1$' config.h; then
    echo 'The builtin CMS compatibility job requires external CMS backends to be disabled.' >&2
    exit 1
fi
make -j"$jobs"
check_release "$baseline" "$build/legacy" 1.8.7-r2
if test -n "$original"; then
    check_release "$original" "$build/original" 1.8.7
fi

check_results()
{
    expected=$1
    # The second argument contains only the repository paths listed here.
    for test_name in $2; do
        awk -v expected="$expected" '
            $1 == ":test-result:" { observations++; if ($2 != expected) bad = 1 }
            $1 == ":global-test-result:" { summaries++; if ($2 != expected) bad = 1 }
            END {
                if (observations == 0 || summaries != 1 || bad) {
                    print "Required " expected " result missing: " FILENAME
                    exit 1
                }
            }
        ' "tests/${test_name%.t}.trs"
    done
}

# Name each mandatory observation so deleting or skipping one fails this job.
set -- \
    dither/fs-gpu/0016_auto_keep_playback.t \
    dither/fs-gpu/0017_auto_composite_playback.t \
    dither/fs-gpu/0018_auto_keep_delta_playback.t \
    cli/quantizer-options/0156_select_color_histgram_retired.t \
    cli/quantizer-options/0157_select_color_histogram_encode.t \
    cli/terminal-color/0040_save_0.t \
    cli/terminal-color/0044_srgb.t \
    cli/terminal-color/0145_srgb_wire.t \
    color/bpc/0022_output_bpc_builtin_float.t \
    color/bpc/0023_output_bpc_libpng_float.t \
    color/bpc/0024_output_bpc_builtin_sixel.t \
    color/bpc/0025_output_bpc_libpng_sixel.t \
    color/bpc/0026_output_bpc_conflict.t \
    color/bpc/0029_output_bpc_auto_tty.t \
    color/bpc/0030_output_bpc_builtin_off_registers.t \
    color/bpc/0031_output_bpc_libpng_off_registers.t \
    color/bpc/0032_output_bpc_builtin_on_registers.t \
    color/bpc/0033_output_bpc_libpng_on_registers.t \
    color/bpc/0034_output_bpc_builtin_off_indices.t \
    color/bpc/0035_output_bpc_libpng_off_indices.t \
    color/bpc/0036_output_bpc_builtin_on_indices.t \
    color/bpc/0037_output_bpc_libpng_on_indices.t \
    color/bpc/0038_output_bpc_builtin_off_float.t \
    color/bpc/0039_output_bpc_libpng_off_float.t \
    color/bpc/0040_output_bpc_builtin_geometry.t \
    color/bpc/0041_output_bpc_libpng_geometry.t \
    color/icc/0005_icc_builtin_device_to_device_intent_paths.t \
    color/icc/0048_absolute_equal_white.t \
    color/icc/0049_absolute_after_missing_relative.t \
    color/icc/0050_absolute_unpaired.t \
    color/icc/0051_mpet_curve_log_range.t \
    color/icc/0052_mpet_curve_sample_endpoint.t \
    color/icc/0053_status_mpet_slot3_supported.t \
    color/icc/0086_mpet_legacy_color.t \
    color/icc/0087_mpet_fallback_from_absolute.t \
    color/icc/0088_mpet_fallback_to_absolute.t \
    color/icc/0089_mpet_matrix_target_intent.t \
    color/icc/0090_mpet_raw_lab_target.t \
    color/icc/0091_colorspace_lab_slot_zero_forward.t \
    color/icc/0092_colorspace_lab_slot_zero_reverse.t \
    color/icc/0093_mpet_high_dimensional_clut.t \
    color/icc/0094_mpet_legacy_slot_pair.t \
    color/icc/0095_mpet_lab_neutral_forward.t \
    color/icc/0096_mpet_lab_neutral_reverse.t \
    color/icc/0097_mpet_reachable_legacy_pair.t \
    color/icc/0098_colorspace_legacy_rgb8.t \
    color/icc/0099_mpet_definition_absolute_fallback_forward.t \
    color/icc/0100_mpet_definition_absolute_fallback_reverse.t \
    color/icc/0101_mpet_rgba_allocation_failure.t \
    color/icc/0102_mpet_rgba_scratch_reuse.t \
    color/icc/0103_mpet_simplex_zero_weight.t \
    color/icc/0104_mpet_lab_pcs_neutral_forward.t \
    color/icc/0105_mpet_lab_pcs_neutral_reverse.t \
    color/icc/0106_colorspace_legacy_gray8.t \
    color/icc/0107_mpet_legacy_reverse_precedence.t \
    color/icc/0108_owned_mpet_matrix_fallback_forward.t \
    color/icc/0109_owned_mpet_matrix_fallback_reverse.t \
    color/icc/0110_mpet_float_storage_fallback.t \
    color/icc/0111_mpet_simplex_cancellation.t \
    color/icc/0112_mpet_simplex_cancellation_chain.t \
    color/icc/0113_mpet_gray_scratch.t \
    color/icc/0114_colorspace_legacy_float_pair.t \
    color/icc/0115_colorspace_legacy_byte_float_pair.t \
    color/icc/0116_mpet_before_later_legacy.t \
    color/icc/0117_mpet_shared_tag_budget.t \
    color/icc/0118_mpet_distinct_tag_budget.t \
    color/icc/0119_mpet_reverse_failure_fallback.t \
    color/icc/0120_mpet_absolute_storage_fallback.t \
    color/icc/0121_mpet_matrix_small_residual.t \
    color/icc/0122_mpet_clut_small_residual.t \
    color/icc/0123_mpet_absolute_forward_snap.t \
    color/icc/0124_mpet_absolute_reverse_snap.t \
    color/icc/0125_mpet_absolute_forward_gamut.t \
    color/icc/0126_mpet_absolute_reverse_gamut.t \
    color/icc/0127_mpet_png_storage_fallback.t \
    color/icc/0128_mpet_curve_small_residual.t \
    color/icc/0129_mpet_legacy_mixed_float.t \
    color/icc/0130_mpet_legacy_mixed_byte.t \
    color/icc/0131_mpet_matrix_product_residual.t \
    color/icc/0132_mpet_simplex_weight_residual.t \
    color/icc/0133_mpet_nonrelative_snap.t \
    color/icc/0134_mpet_nonrelative_gamut.t \
    color/icc/0135_builtin_png_extended_trc.t \
    color/icc/0136_libpng_png_extended_trc.t \
    color/icc/0139_parametric_trc_inverse.t \
    color/icc/0140_sampled_trc_inverse.t \
    color/icc/0141_extended_trc_roundtrip.t \
    color/gamut/0063_pipeline_minde_linear_b2.t \
    color/gamut/0064_pipeline_failure.t \
    color/gamut/0067_pipeline_mask.t \
    color/gamut/0072_pipeline_minde_oklab_b2.t \
    decoder/parser/0106_normal_columns_wide_fallback.t \
    encoder/pipeline/0124_encode_bytes_g1_levels.t \
    encoder/pipeline/0125_encode_bytes_g2_levels.t \
    encoder/pipeline/0126_encode_bytes_g4_levels.t \
    encoder/pipeline/0127_encode_bytes_bgr888_pixels.t \
    encoder/pipeline/0129_encode_frame_g1_levels.t \
    encoder/pipeline/0130_encode_frame_g2_levels.t \
    encoder/pipeline/0131_encode_frame_g4_levels.t \
    encoder/planning/0118_default_colorspace_auto.t \
    encoder/planning/0119_reset_colorspace_auto.t \
    encoder/planning/0120_explicit_clustering_auto_working.t \
    encoder/planning/0121_byte_precision_auto.t \
    encoder/planning/0122_explicit_working_auto_clustering.t \
    encoder/planning/0197_default_display_p3_pixels.t \
    encoder/planning/0198_gamma_display_p3_pixels.t \
    encoder/planning/0199_gamma_display_p3_preserve_resize.t \
    encoder/playback-palette/0025_palette_source_different_primaries.t \
    encoder/playback-palette/0026_palette_source_explicit_srgb.t \
    encoder/playback-palette/0027_palette_source_implicit_srgb.t \
    encoder/playback-palette/0028_palette_source_equal_primaries.t \
    encoder/playback-palette/0029_palette_source_different_transfer.t \
    encoder/playback-palette/0030_palette_source_same_icc.t \
    encoder/playback-palette/0031_palette_source_different_icc.t \
    encoder/playback-palette/0032_gamma_display_p3_palette_pixels.t \
    encoder/playback-palette/0033_explicit_image.t \
    encoder/playback-palette/0034_explicit_source.t \
    encoder/playback-palette/0035_linear_gamma_reject.t \
    encoder/playback-palette/0036_linear_p3_reject.t \
    frame/alpha/0005_frame_alpha_clone.t \
    frame/alpha/0012_frame_alpha_resolve_allocation_atomic.t \
    frame/alpha/0016_frame_float_clone_color.t \
    frame/alpha/0017_pal8_float_paint.t \
    frame/alpha/0018_pal8_float_mask_union.t \
    frame/alpha/0019_pal8_rgb888_mask.t \
    frame/alpha/0020_rgba_float_mask.t \
    frame/alpha/0021_packed_rgb888_mask.t \
    frame/alpha/0022_rgba_rgb888_mask.t \
    frame/alpha/0023_crop_packed_alpha_paint.t \
    frame/alpha/0024_composite_packed_channels.t \
    frame/crop/0010_frame_float_clip_right_atomic.t \
    frame/crop/0077_frame_float_clip_color.t \
    frame/resize/0151_frame_alpha_resize_reject.t \
    frame/resize/0175_frame_float_resize_color.t \
    frame/resize/0176_frame_float_resize_color_retry.t \
    frame/storage/0003_frame_clone_float_canonical.t \
    frame/storage/0011_bgr888_owned.t \
    frame/storage/0012_bgr888_borrowed.t \
    frame/storage/0013_profile_srgb.t \
    frame/storage/0014_profile_p3.t \
    frame/storage/0015_profile_retry.t \
    frame/storage/0016_packed_float_retry.t \
    frame/storage/0017_profile_icc.t \
    frame/storage/0018_raw_pal8_unpainted.t \
    frame/storage/0019_profile_colorspace_lut.t \
    frame/storage/0020_profile_srgb_byte_tie.t \
    frame/storage/0077_public_rgbf32_init_retry.t \
    frame/storage/0078_public_icc_input_direction.t \
    frame/storage/0079_public_rgbf32_owned_slice.t \
    frame/storage/0080_ycbcr422_borrow.t \
    frame/storage/0081_ycbcr422_bounds.t \
    frame/storage/0082_ycbcr422_clone.t \
    frame/storage/0083_ycbcr422_retry.t \
    frame/storage/0084_ycbcr422_unsupported.t \
    frame/storage/0085_ycbcr422_definition.t \
    frame/storage/0021_profile_retained_storage.t \
    frame/storage/0022_segmented_curve_retained_storage.t \
    frame/storage/0023_smptec_float_to_rgbfloat32.t \
    frame/storage/0024_smptec_float_to_rgb888.t \
    frame/storage/0025_smptec_byte_to_rgbfloat32.t \
    frame/storage/0026_smptec_byte_to_rgb888.t \
    frame/storage/0027_display_p3_byte_tie.t \
    frame/storage/0028_builtin_sixel_palette_storage.t \
    frame/storage/0029_d60_neutral_byte_tie.t \
    frame/storage/0030_profile_relative_lut_precedence.t \
    frame/storage/0031_packed_row_storage.t \
    frame/storage/0032_packed_row_clone.t \
    frame/storage/0033_profile_crop_storage.t \
    frame/storage/0034_mask_crop_storage.t \
    frame/storage/0035_alpha_crop_storage.t \
    frame/storage/0036_implicit_pal1_clone.t \
    frame/storage/0037_strip_alpha_crop_storage.t \
    frame/storage/0038_profile_shared_primary_byte_tie.t \
    frame/storage/0039_builtin_tga_compact_storage.t \
    frame/storage/0040_linear_packed_gray_rgb888.t \
    frame/storage/0041_two_shared_primary_tie.t \
    frame/storage/0042_implicit_palette_crop_storage.t \
    frame/alpha/0025_indexed_direct_mask.t \
    frame/resize/0178_linear_dark_byte_resize.t \
    frame/resize/0179_linear_byte_resize_retry.t \
    frame/storage/0043_analytic_linear_identity.t \
    frame/storage/0044_p3_shared_blue_independence.t \
    frame/storage/0045_linear_palette_rgb888.t \
    frame/storage/0046_linear_palette_rgbfloat32.t \
    frame/alpha/0026_strip_linear_coordinates.t \
    palette/fixed/0100_monochrome_sixel_mode_no_palette_slots.t \
    frame/alpha/0027_keyed_palette_registers.t \
    frame/alpha/0028_frame_replacement_cms_history.t \
    frame/storage/0047_linear_index_unpack.t \
    frame/storage/0048_float_to_bgr888.t \
    frame/storage/0049_packed_rgb_float_encoder.t \
    frame/storage/0050_permuted_primaries.t \
    frame/storage/0051_declared_palette_storage.t \
    frame/storage/0052_profiled_destination_range.t \
    frame/storage/0053_bytes_to_bgr888.t \
    frame/storage/0054_padded_rgb_encoder.t \
    palette/fixed/0103_monochrome_auto_gpu.t \
    frame/storage/0055_collinear_primary.t \
    frame/storage/0056_palette_count_before_pointer.t \
    dither/fs-gpu/0019_auto_or_output.t \
    dither/fs-gpu/0020_auto_linear_output.t \
    dither/fs-gpu/0021_auto_smptec_output.t \
    dither/fs-gpu/0022_auto_mask_default.t \
    dither/fs-gpu/0023_auto_mask_clear.t \
    dither/fs-gpu/0024_auto_mask_playback.t \
    frame/storage/0057_default_foreground_pixel.t \
    frame/storage/0058_default_foreground_band.t \
    frame/storage/0059_default_foreground_aspect.t \
    frame/storage/0060_default_foreground_workers.t \
    frame/storage/0061_icc_saturation_matrix.t \
    frame/storage/0062_rgb565_channel_swap.t \
    frame/storage/0063_rgb555_channel_swap.t \
    frame/storage/0064_libpng_compacted_storage.t \
    frame/storage/0065_set_width_storage.t \
    frame/storage/0066_set_height_storage.t \
    frame/storage/0067_packed_rgb_layout_matrix.t \
    frame/storage/0068_packed_rgb_reject.t \
    frame/storage/0069_reshape_storage.t \
    frame/storage/0070_icc_gray_saturation_trc.t \
    frame/storage/0071_borrowed_adoption_storage.t \
    frame/storage/0072_implicit_palette_storage.t \
    loader-png/indexed-color/0014_packed_palette_storage.t \
    loader-png/indexed-color/0015_packed_palette_orientation.t \
    loader-tiff/color-transform/0023_logluv_stonits_scale.t \
    loader/webp/0175_lossless_alpha_linear_output.t \
    frame/resize/0177_profile_byte_resize_retry.t \
    palette/sampling/0027_sample_stream.t \
    palette/sampling/0111_profiled_grid.t \
    palette/sampling/0112_indexed_default_capacity.t \
    palette/output-color/0103_profile_reuse.t \
    palette/output-color/0192_icc_register_nearest_bpc0.t \
    palette/output-color/0193_icc_register_nearest_bpc1.t \
    palette/output-color/0194_icc_register_fs_bpc0.t \
    palette/output-color/0195_icc_register_fs_bpc1.t \
    palette/output-color/0196_icc_register_centroid_linear.t \
    palette/output-color/0197_icc_register_centroid_oklab.t \
    palette/output-color/0198_icc_register_export_original.t \
    palette/output-color/0199_icc_lut_register_nearest.t \
    palette/output-diffusion/0123_icc_lut_register_fs.t \
    palette/output-diffusion/0124_icc_hls_register_fs.t \
    palette/output-diffusion/0125_icc_fixed_playback_register_fs.t \
    palette/output-diffusion/0126_latest_generated_icc_feedback.t \
    palette/output-diffusion/0129_latest_generated_icc_mpet_feedback.t \
    palette/output-diffusion/0130_icc_delta_threshold.t \
    palette/output-diffusion/0131_icc_delta_feedback.t \
    delta/commit/0013_icc_cache_prepare_retry.t \
    delta/commit/0014_icc_cache_copy_retry.t \
    encoder/progressive-refinement/0184_output_icc_context_reject.t \
    cli/progressive/0009_output_icc_reject.t \
    cli/progressive/0010_delta_repair_output_icc_reject.t \
    cli/progressive/0011_icc_progressive_reset_full.t \
    cli/progressive/0012_icc_delta_repair_reset.t \
    cli/progressive/0013_progressive_output_reset_gamma.t \
    cli/progressive/0014_icc_environment_progressive_reset.t \
    encoder/progressive-refinement/0185_output_icc_gamut_retry.t \
    encoder/progressive-refinement/0186_direct_frame_output_icc.t \
    encoder/progressive-refinement/0187_direct_bytes_output_icc.t \
    palette/output-diffusion/0132_icc_delta_working_guard.t \
    palette/output-diffusion/0133_icc_delta_hls_outside.t \
    palette/output-diffusion/0134_icc_delta_hls_staged.t \
    palette/output-diffusion/0135_icc_delta_hls_pipeline.t \
    delta/commit/0015_icc_cache_copy_unrounded.t \
    delta/commit/0016_icc_cache_rounded_inverse_retry.t \
    cli/progressive/0015_rejected_icc_preserves_outfile.t \
    cli/progressive/0016_rejected_delta_icc_preserves_outfile.t \
    cli/progressive/0017_icc_overrides_keep_outfile.t \
    palette/snap-destination/0003_p3_unsnapped_palette_lookup.t \
    palette/working-space/0146_cli_cluster_gamma_centroid.t \
    palette/working-space/0147_cli_cluster_linear_centroid.t \
    palette/working-space/0148_cli_work_gamma_lookup.t \
    palette/working-space/0149_cli_work_linear_lookup.t \
    palette/working-space/0150_cli_work_gamma_fs.t \
    palette/working-space/0151_cli_work_linear_fs.t \
    palette/working-space/0152_oklab_cluster_linear_fs_rgb.t \
    palette/working-space/0153_oklab_cluster_linear_fs_xyz.t \
    palette/working-space/0154_worker_oklab_linear_fs_rgb.t \
    palette/working-space/0155_worker_oklab_linear_fs_xyz.t \
    palette/working-space/0156_matrix_icc_cluster_linear.t \
    palette/working-space/0157_worker_matrix_icc_cluster_oklab.t \
    palette/working-space/0158_cluster_rejects_lut_icc.t \
    palette/working-space/0159_cluster_icc_wire_unsnapped.t \
    palette/working-space/0160_cluster_icc_wire_reversible.t \
    palette/working-space/0161_matrix_icc_auto_binning.t \
    palette/working-space/0162_matrix_icc_reset_auto.t \
    palette/working-space/0163_icc_center_linear_lookup.t \
    palette/working-space/0164_icc_medoids_linear_lookup.t \
    palette/working-space/0165_icc_heckbert_linear_lookup.t \
    palette/working-space/0166_icc_heckbert_linear_fs.t \
    palette/working-space/0167_icc_alpha_serial.t \
    palette/working-space/0168_icc_alpha_worker.t \
    palette/working-space/0169_icc_extended_roundtrip.t \
    palette/working-space/0170_icc_work_lookup.t \
    palette/working-space/0171_icc_work_fs.t \
    palette/working-space/0172_icc_work_auto_cluster.t \
    palette/working-space/0173_icc_work_reset_auto.t \
    palette/working-space/0174_icc_work_rejects_lut.t \
    palette/working-space/0175_icc_work_export.t \
    palette/working-space/0176_icc_work_extended_lookup.t \
    palette/working-space/0177_icc_work_delta_frames.t \
    palette/working-space/0178_icc_work_vptree.t \
    palette/working-space/0179_icc_work_eytzinger.t \
    palette/working-space/0180_icc_work_fhedt_reuse.t \
    palette/working-space/0181_icc_work_parallel_fs.t \
    palette/working-space/0182_icc_work_delta_fs.t \
    palette/working-space/0183_icc_work_fhedt_reserved_key.t \
    palette/working-space/0184_icc_work_pipeline_fs.t \
    palette/working-space/0185_icc_work_delta_threshold.t \
    palette/working-space/0186_icc_work_fhedt_equivalent_profile.t \
    palette/working-space/0187_icc_cluster_work_option_order.t \
    palette/cover/0146_hard_icc_srgb_anchors.t \
    palette/cover/0147_soft_icc_srgb_anchors.t \
    palette/cover/0148_icc_cover_apply_snap.t \
    palette/mapfile-application/0112_structured_map_icc_transition.t \
    palette/export/0007_matrix_icc_heckbert_srgb8.t

# Only the precise observed legacy behavior is an expected failure. Promote
# each witness to the PASS group when its corresponding work is complete.
required_pass="$*"
set -- \
    color/icc/0137_builtin_apng_frame_history.t \
    color/icc/0138_libpng_apng_frame_history.t \
    loader/webp/0176_lossless_icc_alpha_extended.t \
    palette/output-color/0200_continuous_output_target_cluster.t \
    palette/output-diffusion/0127_continuous_output_target_fs.t \
    palette/output-diffusion/0128_generated_output_target_skip.t
# A single invocation keeps every .trs and the combined suite report.
make -j"$jobs" -C tests check TESTS="$required_pass $*"
check_results PASS "$required_pass"
check_results XFAIL "$*"
