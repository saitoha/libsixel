#!/bin/sh
# Verify delta byte sierra2 keep.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0066_delta_byte_sierra2_keep || {
    echo "not ok 1 - delta byte sierra2 keep"
    exit 0
}
echo "ok 1 - delta byte sierra2 keep"
exit 0
