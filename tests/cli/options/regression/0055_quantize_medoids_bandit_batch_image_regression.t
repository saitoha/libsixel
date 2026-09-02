#!/bin/sh
# Verify medoids:bandit_batch preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS|bandit_batch
# Registry binding: quantize_model_kmedoids_bandit_batch|quantize_model_kmedoids_bandit_batch_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0055-quantize-medoids-bandit_batch-short-$$.six"
env_output="${artifact_dir}/0055-quantize-medoids-bandit_batch-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qmedoids:Abandit:X16" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "medoids:bandit_batch short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=bandit_batch|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "bandit_batch short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH=16" -p 16 "-Qmedoids:Abandit" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "medoids:bandit_batch env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=bandit_batch|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "bandit_batch environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "medoids:bandit_batch short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "medoids:bandit_batch image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "medoids:bandit_batch preserves image output"
exit 0
