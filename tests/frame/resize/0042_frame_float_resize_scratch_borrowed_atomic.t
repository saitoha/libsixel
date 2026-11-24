#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame resize scratch borrowed atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state resize 2 || {
    printf '\n%s\n' "not ok 1 - float geometry resize_scratch_borrowed_atomic"
    exit 0
}
printf '\n%s\n' "ok 1 - float geometry resize_scratch_borrowed_atomic"
exit 0
