#!/bin/sh
# Verify GPU wire hls bluenoise.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
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

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-palette/0009_wire_hls_bluenoise || {
    echo "not ok 1 - GPU wire hls bluenoise"
    exit 0
}
echo "ok 1 - GPU wire hls bluenoise"
exit 0
