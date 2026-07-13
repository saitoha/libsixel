#!/bin/sh
# TAP test covering GPU palette apply with 6delta accumulation.
#
# The direct helper keeps this regression focused on the GPU contract:
# unchanged pixels must become the accumulation keycolor and must be recorded
# in the result mask that the encoder later uses to retain the previous RGB
# plane.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -d none -p 16 --lookup-policy=none -o /dev/null "${probe_image}" || {
    printf "1..0 # SKIP forced GPU palette apply is unavailable\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

helper_src="${ARTIFACT_LOCAL_DIR}/gpu-6delta-accumulation.c"
helper_bin="${ARTIFACT_LOCAL_DIR}/gpu-6delta-accumulation${SIXEL_BIN_EXT-}"
runtime_dyld_path="${TOP_BUILDDIR}/src/.libs"
runtime_ld_path="${TOP_BUILDDIR}/src/.libs"

test -z "${DYLD_LIBRARY_PATH+x}" || {
    runtime_dyld_path="${runtime_dyld_path}:${DYLD_LIBRARY_PATH}"
}
test -z "${LD_LIBRARY_PATH+x}" || {
    runtime_ld_path="${runtime_ld_path}:${LD_LIBRARY_PATH}"
}

cat >"${helper_src}" <<'EOF_C'
#include <sixel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/gpu-palette.h"

#define TEST_WIDTH 4
#define TEST_HEIGHT 2
#define TEST_PIXELS (TEST_WIDTH * TEST_HEIGHT)
#define TEST_KEYCOLOR 7

static int
expect_byte(char const *name, unsigned char actual, unsigned char expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(stderr,
            "%s: expected %u, got %u\n",
            name,
            (unsigned int)expected,
            (unsigned int)actual);
    return 0;
}

int
main(void)
{
    SIXELSTATUS status;
    sixel_gpu_palette_request_t request;
    sixel_index_t dest[TEST_PIXELS];
    unsigned char transparent_mask[TEST_PIXELS];
    unsigned char result_mask[TEST_PIXELS];
    unsigned char valid_mask[TEST_PIXELS];
    unsigned char pixels[TEST_PIXELS * 3];
    unsigned char accumulation[TEST_PIXELS * 3];
    unsigned char palette[8 * 3];
    int ok;

    status = SIXEL_FALSE;
    memset(&request, 0, sizeof(request));
    memset(dest, 0xff, sizeof(dest));
    memset(transparent_mask, 0, sizeof(transparent_mask));
    memset(result_mask, 0, sizeof(result_mask));
    memset(valid_mask, 1, sizeof(valid_mask));
    memset(pixels, 0, sizeof(pixels));
    memset(accumulation, 0, sizeof(accumulation));
    memset(palette, 0, sizeof(palette));
    ok = 1;

    palette[1 * 3 + 0] = 255;
    palette[2 * 3 + 1] = 255;
    palette[3 * 3 + 2] = 255;
    palette[4 * 3 + 0] = 255;
    palette[4 * 3 + 1] = 255;
    palette[4 * 3 + 2] = 255;
    palette[TEST_KEYCOLOR * 3 + 0] = 128;
    palette[TEST_KEYCOLOR * 3 + 1] = 128;
    palette[TEST_KEYCOLOR * 3 + 2] = 128;

    pixels[0] = 10;
    pixels[1] = 10;
    pixels[2] = 10;
    accumulation[0] = 12;
    accumulation[1] = 9;
    accumulation[2] = 10;

    pixels[3] = 255;
    pixels[4] = 0;
    pixels[5] = 0;

    pixels[6] = 0;
    pixels[7] = 255;
    pixels[8] = 0;
    transparent_mask[2] = 1;
    result_mask[2] = 1;

    pixels[9] = 10;
    pixels[10] = 10;
    pixels[11] = 10;
    accumulation[9] = 10;
    accumulation[10] = 10;
    accumulation[11] = 10;
    valid_mask[3] = 0;

    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.dest = dest;
    request.pixels = pixels;
    request.pixel_count = TEST_PIXELS;
    request.width = TEST_WIDTH;
    request.height = TEST_HEIGHT;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = sizeof(palette);
    request.palette_depth = 3;
    request.ncolors = 8;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.transparent_mask = transparent_mask;
    request.transparent_mask_size = sizeof(transparent_mask);
    request.transparent_keycolor = TEST_KEYCOLOR;
    request.has_6delta_accumulation = 1;
    request.accumulation_pixels = accumulation;
    request.accumulation_pixels_size = sizeof(accumulation);
    request.accumulation_valid_mask = valid_mask;
    request.accumulation_valid_mask_size = sizeof(valid_mask);
    request.accumulation_keycolor = TEST_KEYCOLOR;
    request.sixdelta_threshold = 3;
    request.accumulation_result_mask = result_mask;
    request.accumulation_result_mask_size = sizeof(result_mask);

    status = sixel_gpu_palette_apply(&request);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "gpu palette apply failed: %04x\n", status);
        return EXIT_FAILURE;
    }

    ok &= expect_byte("kept index", dest[0], TEST_KEYCOLOR);
    ok &= expect_byte("kept mask", result_mask[0], 1);
    ok &= expect_byte("painted index", dest[1], 1);
    ok &= expect_byte("painted mask", result_mask[1], 0);
    ok &= expect_byte("transparent index", dest[2], TEST_KEYCOLOR);
    ok &= expect_byte("transparent mask", result_mask[2], 1);
    ok &= expect_byte("invalid accumulation index", dest[3], 0);
    ok &= expect_byte("invalid accumulation mask", result_mask[3], 0);

    return ok != 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
EOF_C

"${CC:-cc}" \
        -I"${TOP_BUILDDIR}" \
        -I"${TOP_BUILDDIR}/include" \
        -I"${TOP_SRCDIR}/include" \
        -I"${TOP_SRCDIR}" \
        -o "${helper_bin}" \
        "${helper_src}" \
        -L"${TOP_BUILDDIR}/src/.libs" \
        -lsixel \
        "-Wl,-rpath,${TOP_BUILDDIR}/src/.libs" || {
    echo "not ok" 1 - "GPU 6delta helper build failed"
    exit 0
}

DYLD_LIBRARY_PATH="${runtime_dyld_path}" \
LD_LIBRARY_PATH="${runtime_ld_path}" \
${SIXEL_RUNTIME-} "${helper_bin}" || {
    echo "not ok" 1 - "GPU 6delta accumulation result mismatch"
    exit 0
}

echo "ok" 1 - "GPU palette apply records 6delta accumulation"
exit 0
