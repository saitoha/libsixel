#!/bin/sh
# A 64-color limit still forces reduction of the 16x16 source, without
# conflating a severe low-color budget with CLI/env wiring image sanity.
# Verify center:budget_policy preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER|budget_policy
# Registry binding: quantize_model_kcenter_budget_policy|quantize_model_kcenter_budget_policy_override

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
short_output="${artifact_dir}/0076-quantize-center-budget_policy-short-$$.six"
env_output="${artifact_dir}/0076-quantize-center-budget_policy-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 64 "-Qcenter:Dadaptive" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "center:budget_policy short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=budget_policy\|stored=1\|binding=quantize_model_kcenter_budget_policy,quantize_model_kcenter_budget_policy_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "budget_policy short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_BUDGET_POLICY=adaptive" -p 64 "-Qcenter" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "center:budget_policy env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=budget_policy\|stored=1\|binding=quantize_model_kcenter_budget_policy,quantize_model_kcenter_budget_policy_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "budget_policy environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "center:budget_policy short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "center:budget_policy preserves image output"
exit 0
