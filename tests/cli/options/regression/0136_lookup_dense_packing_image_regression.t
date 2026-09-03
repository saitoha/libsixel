#!/bin/sh
# Verify dense packing through both registered lookup bases.
# Registry row: SIXEL_OPTION_SCHEMA_LUT_POLICY|SIXEL_LOOKUP_BASE_SET_DENSE|packing
# Registry binding: lut_policy_packing|lut_policy_packing_override
# Lookup contract: packing=morton

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
short_output="${artifact_dir}/0136-dense-packing-short-$$.six"
env_output="${artifact_dir}/0136-dense-packing-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    "-~5bit:Pmorton" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "5bit packing short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=packing|stored=1|binding=lut_policy_packing,lut_policy_packing_override|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "5bit packing short value was not stored"
    exit 0
}
test "${short_trace#*LSXLUT1|*packing=morton*}" != \
    "${short_trace}" || {
    echo "not ok" 1 - "packing missed the 5bit consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_LOOKUP_PACKING=MoRtOn" "-~5bit" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "5bit packing environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=packing|stored=1|binding=lut_policy_packing,lut_policy_packing_override|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "5bit packing environment value was not stored"
    exit 0
}
test "${env_trace#*LSXLUT1|*packing=morton*}" != \
    "${env_trace}" || {
    echo "not ok" 1 - "environment packing missed the 5bit consumer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "5bit packing outputs differ"
    exit 0
}

bit6_short_trace=$(set +xv; \
    SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    "-~6bit:Pmorton" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "6bit packing short conversion failed"
    exit 0
}
test "${bit6_short_trace#*LSXSUB1|*key=packing|stored=1|binding=lut_policy_packing,lut_policy_packing_override|value=1*}" != "${bit6_short_trace}" || {
    echo "not ok" 1 - "6bit packing short value was not stored"
    exit 0
}
test "${bit6_short_trace#*LSXLUT1|*packing=morton*}" != \
    "${bit6_short_trace}" || {
    echo "not ok" 1 - "packing missed the 6bit consumer"
    exit 0
}

bit6_env_trace=$(set +xv; \
    SIXEL_TRACE_TOPIC=suboption_contract,lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    --env "SIXEL_LOOKUP_PACKING=MoRtOn" "-~6bit" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "6bit packing environment conversion failed"
    exit 0
}
test "${bit6_env_trace#*LSXSUB1|*key=packing|stored=1|binding=lut_policy_packing,lut_policy_packing_override|value=1*}" != "${bit6_env_trace}" || {
    echo "not ok" 1 - "6bit packing environment value was not stored"
    exit 0
}
test "${bit6_env_trace#*LSXLUT1|*packing=morton*}" != \
    "${bit6_env_trace}" || {
    echo "not ok" 1 - "environment packing missed the 6bit consumer"
    exit 0
}

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-~fhedt:Pmorton" \
    "${input_image}" >/dev/null 2>&1
invalid_status=$?
set -e
test "${invalid_status}" -ne 0 || {
    echo "not ok" 1 - "packing escaped its declared lookup bases"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "dense packing image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "dense packing preserves both lookup consumers"
exit 0
