#!/bin/sh
# Verify an invalid backend does not overwrite the selected GPU backend.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "gpu-kmeans/0006_option_atomicity" || {
    echo "not ok 1 - invalid k-means backend is atomic"
    exit 0
}
echo "ok 1 - invalid k-means backend is atomic"
exit 0
