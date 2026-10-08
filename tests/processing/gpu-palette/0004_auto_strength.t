#!/bin/sh
# GPU positional auto uses the current paintable palette size.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Gforce -dnone -p16 \
    -L 'builtin:cms_engine=none!' --precision=8bit -Xgamma -Wgamma \
    -aoff -Fnone '-~none' -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" || {
    echo "1..0 # SKIP forced GPU palette apply is unavailable"
    exit 0
}

echo "1..1"
set -v

SIXEL_DITHER_BLUENOISE_STRENGTH=invalid \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-palette/0004_auto_strength || {
    echo "not ok 1 - GPU auto strength follows paintable palette size"
    exit 0
}

echo "ok 1 - GPU auto strength follows paintable palette size"
exit 0
