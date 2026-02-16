#!/bin/sh
# Inspect ASCII PPM with diffusion and background handling.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ascii_ppm="${TOP_SRCDIR}/images/snake-ascii.ppm"
target_txt="${ARTIFACT_LOCAL_DIR}/ascii-ppm-inspection.txt"

run_img2sixel -I -8 -dburkes -B"#ffffffffffff" "${snake_ascii_ppm}" >"${target_txt}" || {
    fail 1 "ASCII PPM inspection failed"
    exit 0
}

pass 1 "ASCII PPM inspection honours diffusion"
exit 0
