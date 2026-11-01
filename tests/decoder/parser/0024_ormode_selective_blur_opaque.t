#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Policy: docs/decoder/pipeline/README.md
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
