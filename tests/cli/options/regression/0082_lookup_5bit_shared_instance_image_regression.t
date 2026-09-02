#!/bin/sh
# Verify 5bit:shared_instance preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LUT_POLICY|g_lookup_values + SIXEL_LOOKUP_BASE_5BIT|shared_instance
# Registry binding: lut_policy_shared_instance|lut_policy_shared_instance_override
# Lookup contract: shared=1|shared_override=1

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
short_output="${artifact_dir}/0082-lookup-5bit-shared_instance-short-$$.six"
env_output="${artifact_dir}/0082-lookup-5bit-shared_instance-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=2 -p 16 "-~5bit:S1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "5bit:shared_instance short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=shared_instance|stored=1|binding=lut_policy_shared_instance,lut_policy_shared_instance_override*}" != "${short_trace}" || {
    echo "not ok" 1 - "shared_instance short value was not stored"
    exit 0
}

test "${short_trace#*LSXDTH1|*shared=1|shared_override=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "5bit shared_instance did not reach the dither"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOOKUP_5BIT_SHARED_INSTANCE=1" --threads=2 -p 16 "-~5bit" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "5bit:shared_instance env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=shared_instance|stored=1|binding=lut_policy_shared_instance,lut_policy_shared_instance_override*}" != "${env_trace}" || {
    echo "not ok" 1 - "shared_instance environment value was not stored"
    exit 0
}

test "${env_trace#*LSXDTH1|*shared=1|shared_override=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "5bit environment shared_instance did not reach the dither"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "5bit:shared_instance short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "5bit:shared_instance image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "5bit:shared_instance preserves image output"
exit 0
