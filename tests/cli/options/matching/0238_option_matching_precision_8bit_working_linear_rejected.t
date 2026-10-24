#!/bin/sh
# Verify precision-policy=8bit rejects a non-gamma working space.
# Policy: docs/encoder/palette-application/working-colorspace/README.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
status=0
planner_log=$(set +xv; SIXEL_PRECISION_POLICY=auto \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=1 \
    --precision-policy=8bit -Wlinear -dnone -p16 \
    -o/dev/null "${input_image}" 2>&1) || status=$?
test "${status}" -ne 0 || {
    echo "not ok 1 - 8bit with a linear working space was accepted"
    exit 0
}
case "${planner_log}" in
    *'precision-policy=8bit requires the gamma working colorspace'*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - rejection did not name the working colorspace"
    exit 0
}

echo "ok 1 - 8bit rejects a linear working space"
exit 0
