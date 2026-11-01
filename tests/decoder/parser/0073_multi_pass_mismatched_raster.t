#!/bin/sh
# Verify mismatched raster.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0063_multi_pass_mismatched_raster" || {
    echo "not ok 1 - mismatched raster"
    exit 0
}

echo "ok 1 - mismatched raster"
exit 0
