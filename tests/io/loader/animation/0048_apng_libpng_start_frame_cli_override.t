#!/bin/sh
# TAP test: -T overrides an earlier --start-frame option.

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

echo "1..2"
set -v

run_img2sixel --start-frame=0 \
    -Llibpng! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_frame0.six" || {
    fail 1 "APNG decode with --start-frame=0 failed"
    pass 2 "override comparison skipped because baseline decode failed"
    exit 0
}

run_img2sixel --start-frame=1 \
    -Llibpng! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_frame1.six" || {
    fail 1 "APNG decode with --start-frame=1 failed"
    pass 2 "override comparison skipped because frame=1 decode failed"
    exit 0
}

run_img2sixel --start-frame=0 \
    -T 1 -Llibpng! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_cli_override.six" || {
    fail 1 "APNG decode with -T override failed"
    pass 2 "override comparison skipped because -T decode failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/apng_start_frame0.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_start_cli_override.six" && {
    fail 1 "-T did not override earlier --start-frame"
    pass 2 "match check skipped because override did not change output"
    exit 0
}

pass 1 "-T overrides earlier --start-frame selection"

cmp -s "${ARTIFACT_LOCAL_DIR}/apng_start_frame1.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_start_cli_override.six" || {
    fail 2 "-T output does not match equivalent --start-frame"
    exit 0
}

pass 2 "-T output matches equivalent start frame behavior"
exit 0
