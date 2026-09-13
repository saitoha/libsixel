#!/bin/sh
# Verify palette map redefinition.
# Policy: docs/functionality/decoding-pipeline.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0044_palette_map_redefinition" || {
    echo "not ok 1 - 0044_palette_map_redefinition"
    exit 0
}

echo "ok 1 - 0044_palette_map_redefinition"
exit 0
