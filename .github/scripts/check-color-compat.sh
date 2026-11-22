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
    --with-png
grep -qx '#define HAVE_LIBPNG 1' config.h
if grep -Eq '^#define HAVE_(LCMS2|COREGRAPHICS) 1$' config.h; then
    echo 'The builtin CMS compatibility job requires external CMS backends to be disabled.' >&2
    exit 1
fi
make -j"$jobs"
check_release "$baseline" "$build/legacy" 1.8.7-r2
if test -n "$original"; then
    check_release "$original" "$build/original" 1.8.7
fi

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
    decoder/parser/0106_normal_columns_wide_fallback.t \
    encoder/pipeline/0124_encode_bytes_g1_levels.t \
    encoder/pipeline/0125_encode_bytes_g2_levels.t \
    encoder/pipeline/0126_encode_bytes_g4_levels.t \
    encoder/pipeline/0127_encode_bytes_bgr888_pixels.t \
    encoder/pipeline/0129_encode_frame_g1_levels.t \
    encoder/pipeline/0130_encode_frame_g2_levels.t \
    encoder/pipeline/0131_encode_frame_g4_levels.t \
    encoder/planning/0194_default_display_p3_pixels.t \
    encoder/planning/0195_gamma_display_p3_pixels.t \
    encoder/planning/0196_gamma_display_p3_preserve_resize.t \
    encoder/playback-palette/0025_palette_source_different_primaries.t \
    encoder/playback-palette/0026_palette_source_explicit_srgb.t \
    encoder/playback-palette/0027_palette_source_implicit_srgb.t \
    encoder/playback-palette/0028_palette_source_equal_primaries.t \
    encoder/playback-palette/0029_palette_source_different_transfer.t \
    encoder/playback-palette/0030_palette_source_same_icc.t \
    encoder/playback-palette/0031_palette_source_different_icc.t \
    encoder/playback-palette/0032_gamma_display_p3_palette_pixels.t \
    frame/alpha/0005_frame_alpha_clone.t \
    frame/alpha/0012_frame_alpha_resolve_allocation_atomic.t \
    frame/alpha/0016_frame_float_clone_color.t \
    frame/alpha/0017_pal8_float_paint.t \
    frame/alpha/0018_pal8_float_mask_union.t \
    frame/alpha/0019_pal8_rgb888_mask.t \
    frame/alpha/0020_rgba_float_mask.t \
    frame/alpha/0021_packed_rgb888_mask.t \
    frame/alpha/0022_rgba_rgb888_mask.t \
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
    frame/resize/0177_profile_byte_resize_retry.t \
    palette/sampling/0110_profiled_grid.t \
    palette/output-color/0103_profile_reuse.t \
    palette/snap-destination/0003_p3_unsnapped_palette_lookup.t \
    palette/working-space/0146_cli_cluster_gamma_centroid.t \
    palette/working-space/0147_cli_cluster_linear_centroid.t \
    palette/working-space/0148_cli_work_gamma_lookup.t \
    palette/working-space/0149_cli_work_linear_lookup.t \
    palette/working-space/0150_cli_work_gamma_fs.t \
    palette/working-space/0151_cli_work_linear_fs.t
make -j"$jobs" -C tests check TESTS="$*"
for test_name do
    awk '
        $1 == ":test-result:" { observations++; if ($2 != "PASS") bad = 1 }
        $1 == ":global-test-result:" { summaries++; if ($2 != "PASS") bad = 1 }
        END {
            if (observations == 0 || summaries != 1 || bad) {
                print "Required test did not pass: " FILENAME
                exit 1
            }
        }
    ' "tests/${test_name%.t}.trs"
done
