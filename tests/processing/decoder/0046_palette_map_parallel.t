#!/bin/sh
# Verify palette map parallel.
# Policy: docs/functionality/decoding-pipeline.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0046_palette_map_parallel" || {
    echo "not ok 1 - 0046_palette_map_parallel"
    exit 0
}

echo "ok 1 - 0046_palette_map_parallel"
exit 0
