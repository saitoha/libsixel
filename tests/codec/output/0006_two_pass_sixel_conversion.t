#!/bin/sh
# Perform two-pass Sixel conversion to validate re-encoding path.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
stage1="${ARTIFACT_LOCAL_DIR}/two-pass-stage1.sixel"
run_img2sixel -w204 -h204 "${snake_png}" >"${stage1}" || {
    fail 1 "two-pass Sixel conversion fails"
    exit 0
}

run_img2sixel <"${stage1}" >/dev/null || {
    fail 1 "two-pass Sixel conversion fails"
    exit 0
}

pass 1 "two-pass Sixel conversion succeeds"
exit 0
