#!/bin/sh
# Verify center:init_seeds preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER|init_seeds
# Registry binding: quantize_model_kcenter_init_seeds|quantize_model_kcenter_init_seeds_override
# Environment range: parse-unsigned-long

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
short_output="${artifact_dir}/0070-quantize-center-init_seeds-short-$$.six"
env_output="${artifact_dir}/0070-quantize-center-init_seeds-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qcenter:Aswap:N2" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "center:init_seeds short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1\|*key=init_seeds\|stored=1\|binding=quantize_model_kcenter_init_seeds,quantize_model_kcenter_init_seeds_override\|value=2*}" != "${short_trace}" || {
    echo "not ok" 1 - "init_seeds short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_INIT_SEEDS=2" -p 16 "-Qcenter:Aswap" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "center:init_seeds env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1\|*key=init_seeds\|stored=1\|binding=quantize_model_kcenter_init_seeds,quantize_model_kcenter_init_seeds_override\|value=2*}" != "${env_trace}" || {
    echo "not ok" 1 - "init_seeds environment value was not stored"
    exit 0
}

width_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_INIT_SEEDS=4294967296" \
    -p 16 "-Qcenter:Aswap" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "center:init_seeds width conversion failed"
    exit 0
}

test "${width_trace#*LSXSUB1\|*key=init_seeds\|stored=1}" = "${width_trace}" || {
    echo "not ok" 1 - "center init_seeds accepted an over-width value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "center:init_seeds short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "center:init_seeds image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "center:init_seeds preserves image output"
exit 0
