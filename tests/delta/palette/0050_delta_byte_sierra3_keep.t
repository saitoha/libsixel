#!/bin/sh
# Verify delta byte sierra3 keep.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0067_delta_byte_sierra3_keep || {
    printf '\n%s\n' "not ok 1 - delta byte sierra3 keep"
    exit 0
}
printf '\n%s\n' "ok 1 - delta byte sierra3 keep"
exit 0
