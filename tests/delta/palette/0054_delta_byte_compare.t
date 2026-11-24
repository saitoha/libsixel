#!/bin/sh
# Verify delta byte compare.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0071_delta_byte_compare || {
    printf '\n%s\n' "not ok 1 - delta byte compare"
    exit 0
}
printf '\n%s\n' "ok 1 - delta byte compare"
exit 0
