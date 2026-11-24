#!/bin/sh
# Verify delta byte scope output.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0076_delta_byte_scope_output || {
    printf '\n%s\n' "not ok 1 - delta byte scope output"
    exit 0
}
printf '\n%s\n' "ok 1 - delta byte scope output"
exit 0
