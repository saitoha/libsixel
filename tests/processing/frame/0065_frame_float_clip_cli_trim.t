#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify float frame clip cli trim.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state cli 0 || {
    echo "not ok 1 - float geometry clip_cli_trim"
    exit 0
}
echo "ok 1 - float geometry clip_cli_trim"
exit 0
