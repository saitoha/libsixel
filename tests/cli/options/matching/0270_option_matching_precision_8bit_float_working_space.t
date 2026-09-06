#!/bin/sh
# Verify a float-only working space overrides the 8-bit base preference.

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
    --precision=8bit -Wlinear -dnone -p16 \
    -v -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - float-only working-space conversion failed"
    exit 0
}
test "${planner_log#*work=linear-f32*}" != "${planner_log}" || {
    echo "not ok 1 - linear working space did not require float32"
    exit 0
}

echo "ok 1 - float-only working space overrides 8-bit base"
exit 0
