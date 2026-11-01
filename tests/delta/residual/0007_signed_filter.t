#!/bin/sh
# Verify signed filter.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0007_signed_filter || {
    echo "not ok 1 - signed filter"
    exit 0
}
echo "ok 1 - signed filter"
exit 0
