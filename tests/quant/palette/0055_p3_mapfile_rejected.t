#!/bin/sh
# Verify p3 mapfile rejected.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0054_p3_encoder_contract mapfile || {
    echo "not ok 1 - p3 mapfile rejected"
    exit 0
}

echo "ok 1 - p3 mapfile rejected"
exit 0
