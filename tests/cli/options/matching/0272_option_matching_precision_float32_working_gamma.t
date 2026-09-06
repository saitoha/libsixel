#!/bin/sh
# Verify gamma working space preserves an explicit float32 preference.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
planner_log=$(set +xv; SIXEL_FLOAT32_DITHER=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --precision=float32 -Wlinear -Wgamma -dnone -p16 \
    -v -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - float32 gamma conversion failed"
    exit 0
}
test "${planner_log#*work=rgb-f32*}" != "${planner_log}" || {
    echo "not ok 1 - gamma discarded explicit float32 precision"
    exit 0
}

echo "ok 1 - gamma working space preserves float32 precision"
exit 0
