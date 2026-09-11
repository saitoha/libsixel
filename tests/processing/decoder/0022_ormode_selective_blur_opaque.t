#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Policy: docs/functionality/decoding-pipeline.md
# Keep unpainted OR index zero opaque through this dequantizer.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0022_ormode_selective_blur_opaque" || {
    echo "not ok 1 - ormode_selective_blur_opaque"
    exit 0
}

echo "ok 1 - ormode_selective_blur_opaque"
exit 0
