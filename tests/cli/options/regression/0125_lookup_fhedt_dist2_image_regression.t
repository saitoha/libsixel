#!/bin/sh
# Verify fhedt:dist2 through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LUT_POLICY|g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT|dist2
# Registry binding: lut_policy_fhedt_use_dist2|lut_policy_fhedt_use_dist2_override
# Lookup contract: policy=fhedt|*dist2=1

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0135-fhedt-dist2-short-$$.six"
env_output="${artifact_dir}/0135-fhedt-dist2-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    "-~fhedt:D1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "fhedt:dist2 short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=dist2\|stored=1\|binding=lut_policy_fhedt_use_dist2,lut_policy_fhedt_use_dist2_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fhedt dist2 short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXLUT1\|*policy=fhedt\|*dist2=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fhedt dist2 did not reach the policy"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    --env "SIXEL_LOOKUP_FHEDT_USE_DIST2=1" "-~fhedt" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "fhedt:dist2 environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=dist2\|stored=1\|binding=lut_policy_fhedt_use_dist2,lut_policy_fhedt_use_dist2_override\|value=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fhedt dist2 environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXLUT1\|*policy=fhedt\|*dist2=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "environment dist2 missed the policy"
    exit 0
}
float_trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    --precision-policy=float32 "-~fhedt:D1" "${input_image}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "fhedt dist2 float32 conversion failed"
    exit 0
}
case "${float_trace}" in *LSXLUT1\|*policy=fhedt\|precision=float32\|*dist2=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fhedt dist2 missed the float32 backend"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "fhedt dist2 outputs differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "fhedt:dist2 preserves effective image output"
exit 0
