#!/bin/sh
# Verify delta byte scope float.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0077_delta_byte_scope_float || {
    printf '\n%s\n' "not ok 1 - delta byte scope float"
    exit 0
}
printf '\n%s\n' "ok 1 - delta byte scope float"
exit 0
