#!/bin/sh
# TAP test verifying stbn cli motion_adapt overrides invalid env value.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

SIXEL_DITHER_STBN_MOTION_ADAPT=2 \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d stbn:motion_adapt=1 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o /dev/null || {
    echo "not ok" 1 - "stbn cli motion_adapt did not override env value"
    exit 0
}

echo "ok" 1 - "stbn cli motion_adapt overrides env value"
exit 0
