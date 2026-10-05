#!/bin/sh
# Verify delta byte scope body.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0074_delta_byte_scope_body || {
    echo "not ok 1 - delta byte scope body"
    exit 0
}
echo "ok 1 - delta byte scope body"
exit 0
