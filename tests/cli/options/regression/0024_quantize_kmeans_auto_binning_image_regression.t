#!/bin/sh
# Verify automatic Kmeans binning retains the explicit hard-policy output.
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
auto_output="${artifact_dir}/0034-quantize-kmeans-auto-binning-$$.six"
hard_output="${artifact_dir}/0034-quantize-kmeans-hard-binning-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qkmeans \
    "${input_image}" >"${auto_output}" || {
    echo "not ok" 1 - "automatic Kmeans binning conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --binning-policy=hard -Qauto \
    -p 16 -Qkmeans "${input_image}" >"${hard_output}" || {
    echo "not ok" 1 - "explicit hard Kmeans binning conversion failed"
    exit 0
}

cmp -s "${auto_output}" "${hard_output}" || {
    echo "not ok" 1 - "automatic and hard Kmeans binning output differ"
    exit 0
}

# This test owns exact option-route equivalence and consumer selection.
test -s "${auto_output}" || {
    echo "not ok 1 - option-route output is empty"
    exit 0
}

echo "ok" 1 - "automatic Kmeans binning retains hard-policy output"
exit 0
