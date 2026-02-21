#!/bin/sh
# TAP test: APNG blend-over result matches builtin loader output on frame 1.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Llibpng! -S -T 1 \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_blend_over.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_libpng_blend_frame1.six" || {
    fail 1 "APNG libpng blend-over extraction failed"
    exit 0
}

run_img2sixel --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=1" \
    -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_blend_over.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_builtin_blend_frame1.six" || {
    fail 1 "APNG builtin blend-over reference extraction failed"
    exit 0
}

run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${ARTIFACT_LOCAL_DIR}/apng_builtin_blend_frame1.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_libpng_blend_frame1.six" >/dev/null || {
    fail 1 "APNG libpng blend-over frame differs from builtin reference"
    exit 0
}

pass 1 "APNG libpng blend-over frame matches builtin reference"
exit 0
