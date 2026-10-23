#!/bin/sh
# Verify certlut:shared_instance preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_LUT_POLICY|g_lookup_values + SIXEL_LOOKUP_BASE_CERTLUT|shared_instance
# Registry binding: lut_policy_shared_instance|lut_policy_shared_instance_override
# Lookup contract: shared=1|shared_override=1

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
short_output="${artifact_dir}/0084-lookup-certlut-shared_instance-short-$$.six"
env_output="${artifact_dir}/0084-lookup-certlut-shared_instance-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=2 -p 16 "-~certlut:S1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "certlut:shared_instance short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=shared_instance\|stored=1\|binding=lut_policy_shared_instance,lut_policy_shared_instance_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "shared_instance short value was not stored"
    exit 0
}

case "${short_trace}" in *LSXDTH1\|*shared=1\|shared_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "certlut shared_instance did not reach the dither"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE=1" --threads=2 -p 16 "-~certlut" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "certlut:shared_instance env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=shared_instance\|stored=1\|binding=lut_policy_shared_instance,lut_policy_shared_instance_override**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "shared_instance environment value was not stored"
    exit 0
}

case "${env_trace}" in *LSXDTH1\|*shared=1\|shared_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "certlut environment shared_instance did not reach the dither"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "certlut:shared_instance short and env output differ"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "certlut:shared_instance preserves image output"
exit 0
