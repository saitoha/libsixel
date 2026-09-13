#!/bin/sh
# Verify palette map gpu.
# Policy: docs/functionality/decoding-pipeline.md

set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0051_palette_map_gpu" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 - 0051_palette_map_gpu # SKIP GPU dequant unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 0051_palette_map_gpu"
    exit 0
}

echo "ok 1 - 0051_palette_map_gpu"
exit 0
