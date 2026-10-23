#!/bin/sh
# Verify x_dither:strength through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_X_DITHER|strength
# Registry binding: x_dither_strength|x_dither_strength_override
# Dither contract: diffuse=x_dither|*x_strength=0.25|x_strength_override=1

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0124-x-dither-strength-short-$$.six"
env_output="${artifact_dir}/0124-x-dither-strength-env-$$.six"
default_output="${artifact_dir}/0124-x-dither-strength-default-$$.six"
float_short_output="${artifact_dir}/0124-x-dither-strength-float-short-$$.six"
float_env_output="${artifact_dir}/0124-x-dither-strength-float-env-$$.six"
float_default_output="${artifact_dir}/0124-x-dither-strength-float-default-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "x_dither:T0.25" \
    --precision-policy=8bit "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "x_dither:strength short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=strength\|stored=1\|binding=x_dither_strength,x_dither_strength_override\|value=0.25**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "x_dither strength short value was not stored"
    exit 0
}

case "${short_trace}" in *LSXDTH1\|*diffuse=x_dither\|*x_strength=0.25\|x_strength_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "x_dither strength did not reach the dither"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_X_DITHER_STRENGTH=0.25" -d "x_dither" \
    --precision-policy=8bit "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "x_dither:strength environment conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=strength\|stored=1\|binding=x_dither_strength,x_dither_strength_override\|value=0.25**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "x_dither strength environment value was not stored"
    exit 0
}

case "${env_trace}" in *LSXDTH1\|*diffuse=x_dither\|*x_strength=0.25\|x_strength_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "x_dither environment strength did not reach the dither"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "x_dither strength short and environment output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "x_dither" \
    --precision-policy=8bit "${input_image}" >"${default_output}" || {
    echo "not ok" 1 - "x_dither default 8-bit conversion failed"
    exit 0
}

cmp -s "${short_output}" "${default_output}" && {
    echo "not ok" 1 - "x_dither strength did not affect 8-bit output"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "x_dither:T0.25" \
    --precision-policy=float32 "${input_image}" >"${float_short_output}" || {
    echo "not ok" 1 - "x_dither float32 short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_X_DITHER_STRENGTH=0.25" -d "x_dither" \
    --precision-policy=float32 "${input_image}" >"${float_env_output}" || {
    echo "not ok" 1 - "x_dither float32 environment conversion failed"
    exit 0
}

cmp -s "${float_short_output}" "${float_env_output}" || {
    echo "not ok" 1 - "x_dither float32 short and environment output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "x_dither" \
    --precision-policy=float32 "${input_image}" >"${float_default_output}" || {
    echo "not ok" 1 - "x_dither default float32 conversion failed"
    exit 0
}

cmp -s "${float_short_output}" "${float_default_output}" && {
    echo "not ok" 1 - "x_dither strength did not affect float32 output"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

test -s "${float_short_output}" || {
    echo "not ok 1 - float32 option output is empty"
    exit 0
}

echo "ok" 1 - "x_dither:strength preserves effective image output"
exit 0
