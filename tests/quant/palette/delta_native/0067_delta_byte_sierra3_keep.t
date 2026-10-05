#!/bin/sh
# Verify delta byte sierra3 keep.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0067_delta_byte_sierra3_keep || {
    echo "not ok 1 - delta byte sierra3 keep"
    exit 0
}
echo "ok 1 - delta byte sierra3 keep"
exit 0
