#!/bin/sh
# Verify signed filter.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0007_signed_filter || {
    printf '\n%s\n' "not ok 1 - signed filter"
    exit 0
}
printf '\n%s\n' "ok 1 - signed filter"
exit 0
