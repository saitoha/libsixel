#!/bin/sh
# Verify medoids:seed preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS|seed
# Registry binding: quantize_model_kmedoids_seed|quantize_model_kmedoids_seed_override

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0046-quantize-medoids-seed-short-$$.six"
env_output="${artifact_dir}/0046-quantize-medoids-seed-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qmedoids:Arandom:S2" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "medoids:seed short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=seed\|stored=1\|binding=quantize_model_kmedoids_seed,quantize_model_kmedoids_seed_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "seed short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_SEED=2" -p 16 "-Qmedoids:Arandom" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "medoids:seed env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=seed\|stored=1\|binding=quantize_model_kmedoids_seed,quantize_model_kmedoids_seed_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "seed environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "medoids:seed short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "medoids:seed preserves image output"
exit 0
