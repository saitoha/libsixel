#!/bin/sh
# Verify delta byte x dither keep.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0069_delta_byte_x_dither_keep || {
    echo "not ok 1 - delta byte x dither keep"
    exit 0
}
echo "ok 1 - delta byte x dither keep"
exit 0
