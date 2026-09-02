#!/bin/sh
# Verify medoids:bandit_candidates preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS|bandit_candidates
# Registry binding: quantize_model_kmedoids_bandit_candidates|quantize_model_kmedoids_bandit_candidates_override

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
short_output="${artifact_dir}/0054-quantize-medoids-bandit_candidates-short-$$.six"
env_output="${artifact_dir}/0054-quantize-medoids-bandit_candidates-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qmedoids:Abandit:E16" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "medoids:bandit_candidates short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=bandit_candidates|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "bandit_candidates short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES=16" -p 16 "-Qmedoids:Abandit" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "medoids:bandit_candidates env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=bandit_candidates|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "bandit_candidates environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "medoids:bandit_candidates short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "medoids:bandit_candidates image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "medoids:bandit_candidates preserves image output"
exit 0
