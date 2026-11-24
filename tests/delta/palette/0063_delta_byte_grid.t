#!/bin/sh
# Verify every byte candidate against independent nearest-grid arithmetic.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0080_delta_byte_grid || {
    printf '\n%s\n' "not ok 1 - delta byte grid"
    exit 0
}
printf '\n%s\n' "ok 1 - delta byte grid"
exit 0
