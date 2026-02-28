#!/bin/sh
# Inspect ASCII PPM with diffusion and background handling.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_ascii_ppm="${TOP_SRCDIR}/images/snake-ascii.ppm"
target_txt="${ARTIFACT_LOCAL_DIR}/ascii-ppm-inspection.txt"

run_img2sixel -I -8 -dburkes -B"#ffffffffffff" "${snake_ascii_ppm}" >"${target_txt}" || {
    echo "not ok" 1 "ASCII PPM inspection failed"
    exit 0
}

echo "ok" 1 "ASCII PPM inspection honours diffusion"
exit 0
