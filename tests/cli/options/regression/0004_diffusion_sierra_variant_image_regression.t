#!/bin/sh
# Verify sierra:variant preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_SIERRA|variant
# Registry binding: method_for_diffuse
# Dither contract: diffuse=sierra3

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0004-diffusion-sierra-variant-short-$$.six"
env_output="${artifact_dir}/0004-diffusion-sierra-variant-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d "sierra:V3" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "sierra:variant short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=variant|stored=1|binding=method_for_diffuse*}" != "${short_trace}" || {
    echo "not ok" 1 - "variant short value was not stored"
    exit 0
}

test "${short_trace#*LSXDTH1|*diffuse=sierra3*}" != "${short_trace}" || {
    echo "not ok" 1 - "sierra variant did not reach the dither"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_SIERRA_VARIANT=3" -d "sierra" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "sierra:variant env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=variant|stored=1|binding=method_for_diffuse*}" != "${env_trace}" || {
    echo "not ok" 1 - "variant environment value was not stored"
    exit 0
}

test "${env_trace#*LSXDTH1|*diffuse=sierra3*}" != "${env_trace}" || {
    echo "not ok" 1 - "sierra environment variant did not reach the dither"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "sierra:variant short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "sierra:variant image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "sierra:variant preserves image output"
exit 0
