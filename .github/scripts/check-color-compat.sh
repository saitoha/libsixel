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
    decoder/parser/0106_normal_columns_wide_fallback.t \
    encoder/pipeline/0124_encode_bytes_g1_levels.t \
    encoder/pipeline/0125_encode_bytes_g2_levels.t \
    encoder/pipeline/0126_encode_bytes_g4_levels.t \
    encoder/pipeline/0127_encode_bytes_bgr888_pixels.t \
    encoder/planning/0194_default_display_p3_pixels.t \
    encoder/playback-palette/0025_palette_source_different_primaries.t \
    encoder/playback-palette/0026_palette_source_explicit_srgb.t \
    encoder/playback-palette/0027_palette_source_implicit_srgb.t \
    encoder/playback-palette/0028_palette_source_equal_primaries.t \
    encoder/playback-palette/0029_palette_source_different_transfer.t \
    encoder/playback-palette/0030_palette_source_same_icc.t \
    encoder/playback-palette/0031_palette_source_different_icc.t \
    frame/alpha/0005_frame_alpha_clone.t \
    frame/alpha/0012_frame_alpha_resolve_allocation_atomic.t \
    frame/alpha/0016_frame_float_clone_color.t \
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
