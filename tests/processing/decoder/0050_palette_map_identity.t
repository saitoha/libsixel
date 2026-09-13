#!/bin/sh
# Verify palette map identity.
# Policy: docs/functionality/decoding-pipeline.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0050_palette_map_identity" || {
    echo "not ok 1 - 0050_palette_map_identity"
    exit 0
}

echo "ok 1 - 0050_palette_map_identity"
exit 0
