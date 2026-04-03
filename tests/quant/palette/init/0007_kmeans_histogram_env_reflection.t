#!/bin/sh
# TAP test: k-means histogram settings follow environment values.

set -eux


echo "1..1"
set -v

output=$(
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
        --env SIXEL_PALETTE_KMEANS_BINNING=soft \
        --env SIXEL_PALETTE_KMEANS_BINBITS=5 \
        --env SIXEL_PALETTE_KMEANS_MAPPING=srgb \
        --env SIXEL_PALETTE_KMEANS_SOFTDIST=trilinear \
        --env SIXEL_PALETTE_KMEANS_AUTORATIO=64 \
        "palette/0001_kmeans_init" --histogram
) || output=""

cr=$(printf '\r')
test "${output%"${cr}"}" != "${output}" && output=${output%"${cr}"}

test "${output}" = "binning=soft binbits=5 mapping=srgb softdist=trilinear autoratio=64" || {
    echo "not ok" 1 - "unexpected kmeans histogram env output: ${output}"
    exit 0
}

echo "ok" 1 - "kmeans histogram environment mapping matched"
exit 0
