#!/bin/sh
# Verify mapped palette during parallel fallback.
# Policy: docs/functionality/decoding-pipeline.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0052_palette_map_parallel_fallback" || {
    echo "not ok 1 - 0052_palette_map_parallel_fallback"
    exit 0
}

echo "ok 1 - 0052_palette_map_parallel_fallback"
exit 0
