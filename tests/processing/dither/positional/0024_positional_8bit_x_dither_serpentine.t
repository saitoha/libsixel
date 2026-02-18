#!/bin/sh
# TAP test covering positional 8-bit x_dither with serpentine scan.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel -d x_dither -y serpentine --precision=8bit -p 16 \
        -o "${output_sixel}" "${input_image}" || {
    fail 1 "positional 8-bit x_dither serpentine failed"
    exit 0
}

pass 1 "positional 8-bit x_dither serpentine passed"

exit 0
