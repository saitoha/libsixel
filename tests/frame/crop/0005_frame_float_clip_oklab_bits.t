#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame clip oklab bits.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state clip 2 || {
    printf '\n%s\n' "not ok 1 - float geometry clip_oklab_bits"
    exit 0
}
printf '\n%s\n' "ok 1 - float geometry clip_oklab_bits"
exit 0
