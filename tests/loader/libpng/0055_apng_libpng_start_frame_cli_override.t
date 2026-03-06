#!/bin/sh
# TAP test: -T overrides an earlier --start-frame option.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle --start-frame=0 \
    -Llibpng! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_frame0.six" || {
    echo "not ok" 1 - "APNG decode with --start-frame=0 failed"
    exit 0
}

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle --start-frame=0 \
    -T 1 -Llibpng! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_cli_override.six" || {
    echo "not ok" 1 - "APNG decode with -T override failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/apng_start_frame0.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_start_cli_override.six" && {
    echo "not ok" 1 - "-T did not override earlier --start-frame"
    exit 0
}

echo "ok" 1 - "-T overrides earlier --start-frame selection"
exit 0
