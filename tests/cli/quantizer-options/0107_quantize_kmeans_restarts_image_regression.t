#!/bin/sh
# Verify kmeans:restarts preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS|restarts
# Registry binding: quantize_model_kmeans_restarts|quantize_model_kmeans_restarts_override
# Environment range: clamp-unsigned-uint

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
short_output="${artifact_dir}/0038-quantize-kmeans-restarts-short-$$.six"
env_output="${artifact_dir}/0038-quantize-kmeans-restarts-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qkmeans:E32" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "kmeans:restarts short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=restarts\|stored=1\|binding=quantize_model_kmeans_restarts,quantize_model_kmeans_restarts_override\|value=32**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "restarts short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEANS_RESTARTS=-1" -p 16 "-Qkmeans" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "kmeans:restarts env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=restarts\|stored=1\|binding=quantize_model_kmeans_restarts,quantize_model_kmeans_restarts_override\|value=32**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "restarts environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEANS_RESTARTS=0" \
    -p 16 "-Qkmeans" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "kmeans:restarts lower conversion failed"
    exit 0
}

case "${range_trace}" in *LSXSUB1\|*key=restarts\|stored=1\|binding=quantize_model_kmeans_restarts,quantize_model_kmeans_restarts_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "restarts lower endpoint was not clamped"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "kmeans:restarts short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "kmeans:restarts preserves image output"
exit 0
