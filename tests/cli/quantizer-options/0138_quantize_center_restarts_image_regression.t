#!/bin/sh
# A 64-color limit still forces reduction of the 16x16 source, without
# conflating a severe low-color budget with CLI/env wiring image sanity.
# Verify center:restarts preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER|restarts
# Registry binding: quantize_model_kcenter_restarts|quantize_model_kcenter_restarts_override
# Environment range: parse-unsigned-long

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
short_output="${artifact_dir}/0069-quantize-center-restarts-short-$$.six"
env_output="${artifact_dir}/0069-quantize-center-restarts-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 64 "-Qcenter:X2" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "center:restarts short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=restarts\|stored=1\|binding=quantize_model_kcenter_restarts,quantize_model_kcenter_restarts_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "restarts short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_RESTARTS=2" -p 64 "-Qcenter" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "center:restarts env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=restarts\|stored=1\|binding=quantize_model_kcenter_restarts,quantize_model_kcenter_restarts_override\|value=2**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "restarts environment value was not stored"
    exit 0
}

width_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_RESTARTS=4294967296" \
    -p 64 "-Qcenter" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "center:restarts width conversion failed"
    exit 0
}

case "${width_trace}" in *LSXSUB1\|*key=restarts\|stored=1*) false ;; *) true ;; esac || {
    echo "not ok" 1 - "center restarts accepted an over-width value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "center:restarts short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "center:restarts preserves image output"
exit 0
