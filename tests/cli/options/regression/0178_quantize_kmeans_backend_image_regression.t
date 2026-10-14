#!/bin/sh
# Verify kmeans:backend preserves image output through short and env paths.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS|backend
# Registry binding: quantize_model_kmeans_backend|quantize_model_kmeans_backend_override

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

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0175-quantize-kmeans-backend-short-$$.six"
env_output="${artifact_dir}/0175-quantize-kmeans-backend-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Gauto -p 256 "-Qkmeans:Ggpu" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "kmeans:backend short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=backend\|stored=1\|binding=quantize_model_kmeans_backend,quantize_model_kmeans_backend_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "backend short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Gauto -p 256 --env "SIXEL_PALETTE_KMEANS_BACKEND=gpu" "-Qkmeans" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "kmeans:backend env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=backend\|stored=1\|binding=quantize_model_kmeans_backend,quantize_model_kmeans_backend_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "backend environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "kmeans:backend short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "kmeans:backend preserves image output"
exit 0
