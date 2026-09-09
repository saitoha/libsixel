#!/bin/sh
# Verify medoids:bandit_iter preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS|bandit_iter
# Registry binding: quantize_model_kmedoids_bandit_iter|quantize_model_kmedoids_bandit_iter_override
# Environment range: clamp-positive-uint

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0053-quantize-medoids-bandit_iter-short-$$.six"
env_output="${artifact_dir}/0053-quantize-medoids-bandit_iter-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qmedoids:Abandit:D64" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "medoids:bandit_iter short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1\|*key=bandit_iter\|stored=1\|binding=quantize_model_kmedoids_bandit_iter,quantize_model_kmedoids_bandit_iter_override\|value=64*}" != "${short_trace}" || {
    echo "not ok" 1 - "bandit_iter short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER=65" -p 16 "-Qmedoids:Abandit" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "medoids:bandit_iter env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1\|*key=bandit_iter\|stored=1\|binding=quantize_model_kmedoids_bandit_iter,quantize_model_kmedoids_bandit_iter_override\|value=64*}" != "${env_trace}" || {
    echo "not ok" 1 - "bandit_iter environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER=0" \
    -p 16 "-Qmedoids:Abandit" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "medoids:bandit_iter lower conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1\|*key=bandit_iter\|stored=1}" = "${range_trace}" || {
    echo "not ok" 1 - "medoids bandit_iter accepted zero"
    exit 0
}

width_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER=4294967296" \
    -p 16 "-Qmedoids:Abandit" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "medoids:bandit_iter width conversion failed"
    exit 0
}

test "${width_trace#*LSXSUB1\|*key=bandit_iter\|stored=1}" = "${width_trace}" || {
    echo "not ok" 1 - "medoids bandit_iter accepted an over-width value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "medoids:bandit_iter short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "medoids:bandit_iter image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "medoids:bandit_iter preserves image output"
exit 0
