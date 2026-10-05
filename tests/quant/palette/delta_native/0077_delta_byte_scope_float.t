#!/bin/sh
# Verify delta byte scope float.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0077_delta_byte_scope_float || {
    echo "not ok 1 - delta byte scope float"
    exit 0
}
echo "ok 1 - delta byte scope float"
exit 0
