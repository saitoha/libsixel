#!/bin/sh
# Verify filter threshold boundary.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0008_filter_threshold_boundary || {
    echo "not ok 1 - filter threshold boundary"
    exit 0
}
echo "ok 1 - filter threshold boundary"
exit 0
