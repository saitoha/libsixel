#!/bin/sh
# Verify default palette direct.
# Policy: docs/functionality/decoding-pipeline.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0042_default_palette_direct" || {
    echo "not ok 1 - 0042_default_palette_direct"
    exit 0
}

echo "ok 1 - 0042_default_palette_direct"
exit 0
