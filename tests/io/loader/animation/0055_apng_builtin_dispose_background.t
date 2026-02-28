#!/bin/sh
# TAP test: builtin APNG dispose-background static rendering matches reference.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_dispose_background.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_dispose_background_builtin_static.six" || {
    fail 1 "builtin APNG dispose-background static rendering failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_dispose_background_builtin_static_reference.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_dispose_background_builtin_static.six" 2>&1) || {
    fail 1 "${lsqa_msg}"
    exit 0
}

pass 1 "builtin APNG dispose-background static rendering matches reference"
exit 0
