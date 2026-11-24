#!/bin/sh
# Verify an invalid backend does not overwrite the selected GPU backend.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "gpu-kmeans/0006_option_atomicity" || {
    printf '\n%s\n' "not ok 1 - invalid k-means backend is atomic"
    exit 0
}
printf '\n%s\n' "ok 1 - invalid k-means backend is atomic"
exit 0
