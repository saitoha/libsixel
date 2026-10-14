#!/bin/sh
# Verify x strength precedence.
# Policy: docs/encoder/palette-application/dithering/README.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
    -d none -p 16 --lookup-policy=none -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" || {
    echo "1..0 # SKIP forced GPU palette apply is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-palette/0017_x_strength_precedence || {
    echo "not ok 1 - x strength precedence"
    exit 0
}
echo "ok 1 - x strength precedence"
exit 0
