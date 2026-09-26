#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame byte boundary: cielab clone interface numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0178_float_byte_boundary 3 4 0 1>&2 || {
    echo "not ok 1 - frame byte cielab clone interface numeric"
    exit 0
}
echo "ok 1 - frame byte cielab clone interface numeric"
exit 0
