#!/bin/sh
# Verify compare change preserves rgb.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0067_compare_change_preserves_rgb" >/dev/null || {
    echo "not ok 1 - compare change preserves rgb"
    exit 0
}

echo "ok 1 - compare change preserves rgb"
exit 0
