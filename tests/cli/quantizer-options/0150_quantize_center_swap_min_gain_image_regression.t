#!/bin/sh
# A 64-color limit still forces reduction of the 16x16 source, without
# conflating a severe low-color budget with CLI/env wiring image sanity.
# Verify center:swap_min_gain preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER|swap_min_gain
# Registry binding: quantize_model_kcenter_swap_min_gain|quantize_model_kcenter_swap_min_gain_override

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
short_output="${artifact_dir}/0081-quantize-center-swap_min_gain-short-$$.six"
env_output="${artifact_dir}/0081-quantize-center-swap_min_gain-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 64 "-Qcenter:Aswap:J0.1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "center:swap_min_gain short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=swap_min_gain\|stored=1\|binding=quantize_model_kcenter_swap_min_gain,quantize_model_kcenter_swap_min_gain_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "swap_min_gain short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_SWAP_MIN_GAIN=0.1" -p 64 "-Qcenter:Aswap" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "center:swap_min_gain env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=swap_min_gain\|stored=1\|binding=quantize_model_kcenter_swap_min_gain,quantize_model_kcenter_swap_min_gain_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "swap_min_gain environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "center:swap_min_gain short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "center:swap_min_gain preserves image output"
exit 0
