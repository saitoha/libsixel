#!/bin/sh
# Verify kmeans:softdist preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS|softdist
# Registry binding: quantize_model_kmeans_softdist_mode|quantize_model_kmeans_softdist_override

# Image assertion: option-values-and-output-equivalence
set -eux

# OpenBSD rand() is non-deterministic by default, so pin the unrelated
# Kmeans initializer while comparing independent converter processes.
SIXEL_PALETTE_KMEANS_SEED=1
export SIXEL_PALETTE_KMEANS_SEED

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0033-quantize-kmeans-softdist-short-$$.six"
env_output="${artifact_dir}/0033-quantize-kmeans-softdist-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --binning-policy=soft -Qauto -p 16 "-Qkmeans:Dtrilinear" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "kmeans:softdist short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=softdist\|stored=1\|binding=quantize_model_kmeans_softdist_mode,quantize_model_kmeans_softdist_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "softdist short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --binning-policy=soft -Qauto \
    --env "SIXEL_PALETTE_KMEANS_SOFTDIST=trilinear" -p 16 -Qkmeans \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "kmeans:softdist env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=softdist\|stored=1\|binding=quantize_model_kmeans_softdist_mode,quantize_model_kmeans_softdist_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "softdist environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "kmeans:softdist short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "kmeans:softdist preserves image output"
exit 0
