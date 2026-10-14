#!/bin/sh
# Verify medoids:bandit_batch preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS|bandit_batch
# Registry binding: quantize_model_kmedoids_bandit_batch|quantize_model_kmedoids_bandit_batch_override
# Environment range: clamp-positive-uint

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
short_output="${artifact_dir}/0055-quantize-medoids-bandit_batch-short-$$.six"
env_output="${artifact_dir}/0055-quantize-medoids-bandit_batch-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qmedoids:Abandit:X4096" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "medoids:bandit_batch short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=bandit_batch\|stored=1\|binding=quantize_model_kmedoids_bandit_batch,quantize_model_kmedoids_bandit_batch_override\|value=4096**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "bandit_batch short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH=4097" -p 16 "-Qmedoids:Abandit" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "medoids:bandit_batch env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=bandit_batch\|stored=1\|binding=quantize_model_kmedoids_bandit_batch,quantize_model_kmedoids_bandit_batch_override\|value=4096**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "bandit_batch environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH=0" \
    -p 16 "-Qmedoids:Abandit" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "medoids:bandit_batch lower conversion failed"
    exit 0
}

case "${range_trace}" in *LSXSUB1\|*key=bandit_batch\|stored=1*) false ;; *) true ;; esac || {
    echo "not ok" 1 - "medoids bandit_batch accepted zero"
    exit 0
}

width_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH=4294967296" \
    -p 16 "-Qmedoids:Abandit" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "medoids:bandit_batch width conversion failed"
    exit 0
}

case "${width_trace}" in *LSXSUB1\|*key=bandit_batch\|stored=1*) false ;; *) true ;; esac || {
    echo "not ok" 1 - "medoids bandit_batch accepted an over-width value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "medoids:bandit_batch short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "medoids:bandit_batch preserves image output"
exit 0
