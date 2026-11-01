#!/bin/sh
# Verify delta byte jajuni keep.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0062_delta_byte_jajuni_keep || {
    echo "not ok 1 - delta byte jajuni keep"
    exit 0
}
echo "ok 1 - delta byte jajuni keep"
exit 0
