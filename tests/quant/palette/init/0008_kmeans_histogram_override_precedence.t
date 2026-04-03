#!/bin/sh
# TAP test: explicit k-means histogram overrides take precedence over env.

set -eux


echo "1..1"
set -v

output=$(
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
        --env SIXEL_PALETTE_KMEANS_BINNING=soft \
        --env SIXEL_PALETTE_KMEANS_BINBITS=8 \
        --env SIXEL_PALETTE_KMEANS_MAPPING=srgb \
        --env SIXEL_PALETTE_KMEANS_SOFTDIST=trilinear \
        --env SIXEL_PALETTE_KMEANS_AUTORATIO=99 \
        --env SIXEL_PALETTE_KMEANS_FEEDBACK=on \
        "palette/0001_kmeans_init" --histogram-override
) || output=""

cr=$(printf '\r')
test "${output%"${cr}"}" != "${output}" && output=${output%"${cr}"}

test "${output}" = "binning=hard binbits=7 mapping=uniform softdist=trilinear autoratio=17 feedback=off" || {
    echo "not ok" 1 - "unexpected kmeans override output: ${output}"
    exit 0
}

echo "ok" 1 - "kmeans overrides take precedence over environment"
exit 0
