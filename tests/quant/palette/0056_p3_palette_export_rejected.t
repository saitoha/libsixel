#!/bin/sh
# Verify p3 palette export rejected.
# Policy: docs/functionality/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0054_p3_encoder_contract export || {
    echo "not ok 1 - p3 palette export rejected"
    exit 0
}

echo "ok 1 - p3 palette export rejected"
exit 0
