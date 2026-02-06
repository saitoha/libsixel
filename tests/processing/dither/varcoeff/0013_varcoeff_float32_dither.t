#!/bin/sh
# TAP test ensuring LSO2 dither exercises the float32 variable-coefficient path.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/varcoeff-float32.six"

if run_img2sixel -d lso2 -y serpentine --precision=float32 -p 16 \
        -o "${output_sixel}" "${snake_png}"; then
    pass 1 "variable-coefficient float32 dither completes"
else
    fail 1 "variable-coefficient float32 dither failed"
fi

exit "${status}"
