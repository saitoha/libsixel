#!/bin/sh
# Verify center:prune_mass preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER|prune_mass
# Registry binding: quantize_model_kcenter_prune_mass|quantize_model_kcenter_prune_mass_override

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
short_output="${artifact_dir}/0075-quantize-center-prune_mass-short-$$.six"
env_output="${artifact_dir}/0075-quantize-center-prune_mass-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qcenter:U0.95" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "center:prune_mass short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=prune_mass|stored=1|binding=quantize_model_kcenter_prune_mass,quantize_model_kcenter_prune_mass_override*}" != "${short_trace}" || {
    echo "not ok" 1 - "prune_mass short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_PRUNE_MASS=0.95" -p 16 "-Qcenter" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "center:prune_mass env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=prune_mass|stored=1|binding=quantize_model_kcenter_prune_mass,quantize_model_kcenter_prune_mass_override*}" != "${env_trace}" || {
    echo "not ok" 1 - "prune_mass environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "center:prune_mass short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "center:prune_mass image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "center:prune_mass preserves image output"
exit 0
