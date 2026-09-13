#!/bin/sh
# Verify palette map definitions.
# Policy: docs/functionality/decoding-pipeline.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0043_palette_map_definitions" || {
    echo "not ok 1 - 0043_palette_map_definitions"
    exit 0
}

echo "ok 1 - 0043_palette_map_definitions"
exit 0
