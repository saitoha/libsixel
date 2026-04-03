#!/bin/sh
# TAP test: k-means histogram settings default when env is unset.

set -eux


echo "1..1"
set -v

output=$(
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
        --env SIXEL_PALETTE_KMEANS_BINNING= \
        --env SIXEL_PALETTE_KMEANS_BINBITS= \
        --env SIXEL_PALETTE_KMEANS_MAPPING= \
        --env SIXEL_PALETTE_KMEANS_SOFTDIST= \
        --env SIXEL_PALETTE_KMEANS_AUTORATIO= \
        "palette/0001_kmeans_init" --histogram
) || output=""

cr=$(printf '\r')
test "${output%"${cr}"}" != "${output}" && output=${output%"${cr}"}

test "${output}" = "binning=auto binbits=6 mapping=uniform softdist=trilinear autoratio=32" || {
    echo "not ok" 1 - "unexpected kmeans histogram defaults: ${output}"
    exit 0
}

echo "ok" 1 - "kmeans histogram defaults matched"
exit 0
