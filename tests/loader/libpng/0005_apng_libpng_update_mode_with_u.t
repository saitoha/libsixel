#!/bin/sh
# TAP test: APNG update rectangles match builtin loader output on frame 1.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Llibpng! -S -T 1 \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_libpng_update_frame1.six" || {
    echo "not ok" 1 - "APNG libpng frame extraction failed"
    exit 0
}

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=1" \
    -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_builtin_update_frame1.six" || {
    echo "not ok" 1 - "APNG builtin reference extraction failed"
    exit 0
}

run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${ARTIFACT_LOCAL_DIR}/apng_builtin_update_frame1.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_libpng_update_frame1.six" >/dev/null || {
    echo "not ok" 1 - "APNG libpng update frame differs from builtin reference"
    exit 0
}

echo "ok" 1 - "APNG libpng update frame matches builtin reference"
exit 0
