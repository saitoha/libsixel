#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify top-level adaptive sampling agrees across short and environment paths.
# Top-level option regression

# Image assertion: option-values-and-output-equivalence
set -eux

# OpenBSD rand() is non-deterministic by default, so pin the unrelated
# Kmeans initializer while comparing independent converter processes.
SIXEL_PALETTE_KMEANS_SEED=1
export SIXEL_PALETTE_KMEANS_SEED

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0162-sampling-policy-short-$$.six"
env_output="${artifact_dir}/0162-sampling-policy-env-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -4adaptive-grid \
    -p 16 -Qkmeans "${input_image}" >"${short_output}" || {
    echo "not ok" 1 - "short adaptive-grid conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_SAMPLING_POLICY=adaptive-grid" -p 16 -Qkmeans \
    "${input_image}" >"${env_output}" || {
    echo "not ok" 1 - "environment adaptive-grid conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "short and environment sampling output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "short top-level sampling preserves image output"
exit 0
