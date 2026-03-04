#!/bin/sh
# TAP test: -T override output matches equivalent --start-frame selection.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle --start-frame=1 \
    -Llibpng! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_frame1.six" || {
    echo "not ok" 1 "APNG decode with --start-frame=1 failed"
    exit 0
}

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle --start-frame=0 \
    -T 1 -Llibpng! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_cli_override.six" || {
    echo "not ok" 1 "APNG decode with -T override failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/apng_start_frame1.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_start_cli_override.six" || {
    echo "not ok" 1 "-T output does not match equivalent --start-frame"
    exit 0
}

echo "ok" 1 "-T output matches equivalent start frame behavior"
exit 0
