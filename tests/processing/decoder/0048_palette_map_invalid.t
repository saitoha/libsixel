#!/bin/sh
# Verify palette map invalid.
# Policy: docs/functionality/decoding-pipeline.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0048_palette_map_invalid" || {
    echo "not ok 1 - 0048_palette_map_invalid"
    exit 0
}

echo "ok 1 - 0048_palette_map_invalid"
exit 0
