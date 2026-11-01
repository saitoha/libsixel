#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame resize mask source borrowed atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state resize 3 || {
    echo "not ok 1 - float geometry resize_mask_source_borrowed_atomic"
    exit 0
}
echo "ok 1 - float geometry resize_mask_source_borrowed_atomic"
exit 0
