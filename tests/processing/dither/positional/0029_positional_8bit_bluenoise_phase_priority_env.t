#!/bin/sh
# TAP test covering positional 8-bit bluenoise phase priority over seed.
#
# The helper's --env parser uses commas to separate assignments, so this test
# intentionally uses a phase token without commas to keep one-file-one-case
# behavior while still exercising the phase_set path that suppresses seed use.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel --env SIXEL_DITHER_BLUENOISE_PHASE=10 \
        --env SIXEL_DITHER_BLUENOISE_SEED=123 -d bluenoise -y raster \
        --precision=8bit -p 16 -o "${output_sixel}" "${input_image}" || {
    fail 1 "positional 8-bit bluenoise phase priority env failed"
    exit 0
}

pass 1 "positional 8-bit bluenoise phase priority env passed"

exit 0
