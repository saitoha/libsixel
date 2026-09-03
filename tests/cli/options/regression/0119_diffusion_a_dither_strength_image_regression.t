#!/bin/sh
# Verify a_dither:strength through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_A_DITHER|strength
# Registry binding: a_dither_strength|a_dither_strength_override
# Dither contract: diffuse=a_dither|*a_strength=0.25|a_strength_override=1

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
reference_image="${input_image}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0119-a-dither-strength-short-$$.six"
env_output="${artifact_dir}/0119-a-dither-strength-env-$$.six"
default_output="${artifact_dir}/0119-a-dither-strength-default-$$.six"
float_short_output="${artifact_dir}/0119-a-dither-strength-float-short-$$.six"
float_env_output="${artifact_dir}/0119-a-dither-strength-float-env-$$.six"
float_default_output="${artifact_dir}/0119-a-dither-strength-float-default-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "a_dither:T0.25" \
    --precision=8bit "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "a_dither:strength short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=strength|stored=1|binding=a_dither_strength,a_dither_strength_override|value=0.25*}" != "${short_trace}" || {
    echo "not ok" 1 - "a_dither strength short value was not stored"
    exit 0
}

test "${short_trace#*LSXDTH1|*diffuse=a_dither|*a_strength=0.25|a_strength_override=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "a_dither strength did not reach the dither"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_A_DITHER_STRENGTH=0.25" -d "a_dither" \
    --precision=8bit "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "a_dither:strength environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=strength|stored=1|binding=a_dither_strength,a_dither_strength_override|value=0.25*}" != "${env_trace}" || {
    echo "not ok" 1 - "a_dither strength environment value was not stored"
    exit 0
}

test "${env_trace#*LSXDTH1|*diffuse=a_dither|*a_strength=0.25|a_strength_override=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "a_dither environment strength did not reach the dither"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "a_dither strength short and environment output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "a_dither" \
    --precision=8bit "${input_image}" >"${default_output}" || {
    echo "not ok" 1 - "a_dither default 8-bit conversion failed"
    exit 0
}

cmp -s "${short_output}" "${default_output}" && {
    echo "not ok" 1 - "a_dither strength did not affect 8-bit output"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "a_dither:T0.25" \
    --precision=float32 "${input_image}" >"${float_short_output}" || {
    echo "not ok" 1 - "a_dither float32 short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_A_DITHER_STRENGTH=0.25" -d "a_dither" \
    --precision=float32 "${input_image}" >"${float_env_output}" || {
    echo "not ok" 1 - "a_dither float32 environment conversion failed"
    exit 0
}

cmp -s "${float_short_output}" "${float_env_output}" || {
    echo "not ok" 1 - "a_dither float32 short and environment output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "a_dither" \
    --precision=float32 "${input_image}" >"${float_default_output}" || {
    echo "not ok" 1 - "a_dither default float32 conversion failed"
    exit 0
}

cmp -s "${float_short_output}" "${float_default_output}" && {
    echo "not ok" 1 - "a_dither strength did not affect float32 output"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "a_dither strength 8-bit image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

float_lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${float_short_output}" 2>&1) || float_lsqa_status=$?
test "${float_lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "a_dither strength float32 image quality regressed"
    printf "# %s\n" "${float_lsqa_error}"
    exit 0
}

echo "ok" 1 - "a_dither:strength preserves effective image output"
exit 0
