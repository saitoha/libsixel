#!/bin/sh
# TAP test: libpng APNG blend-over start frame matches static reference.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Llibpng! -S --start-frame=1 \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_blend_over.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_blend_over_libpng_frame1.six" || {
    echo "not ok" 1 "libpng APNG blend-over frame extraction failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_blend_over_libpng_start_frame1_reference.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_blend_over_libpng_frame1.six" 2>&1) || {
    echo "not ok" 1 "${lsqa_msg}"
    exit 0
}

echo "ok" 1 "libpng APNG blend-over frame matches static reference"
exit 0
