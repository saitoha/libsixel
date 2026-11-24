#!/bin/sh
# Byte K-means keeps byte-only palette storage.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/kmeans_byte" || {
    printf '\n%s\n' "not ok 1 - Byte K-means keeps byte-only palette storage"
    exit 0
}
printf '\n%s\n' "ok 1 - Byte K-means keeps byte-only palette storage"
exit 0
