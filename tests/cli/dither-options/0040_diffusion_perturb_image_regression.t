#!/bin/sh
# Verify fs:perturb through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|NULL|perturb
# Registry binding: diffusion_perturb|diffusion_perturb_override
# Dither contract: diffuse=fs|*perturb=0.5|perturb_override=1

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
short_output="${artifact_dir}/0165-perturb-short-$$.six"
env_output="${artifact_dir}/0165-perturb-env-$$.six"
default_output="${artifact_dir}/0165-perturb-default-$$.six"
float_short_output="${artifact_dir}/0165-perturb-float-short-$$.six"
float_env_output="${artifact_dir}/0165-perturb-float-env-$$.six"
float_default_output="${artifact_dir}/0165-perturb-float-default-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs:U0.5" \
    --precision-policy=8bit "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "fs:perturb short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=perturb\|stored=1\|binding=diffusion_perturb,diffusion_perturb_override\|value=0.5**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fs perturb short value was not stored"
    exit 0
}

case "${short_trace}" in *LSXDTH1\|*diffuse=fs\|*perturb=0.5\|perturb_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fs perturb did not reach the dither"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_PERTURB=0.5" -d "fs" \
    --precision-policy=8bit "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "fs:perturb environment conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=perturb\|stored=1\|binding=diffusion_perturb,diffusion_perturb_override\|value=0.5**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fs perturb environment value was not stored"
    exit 0
}

case "${env_trace}" in *LSXDTH1\|*diffuse=fs\|*perturb=0.5\|perturb_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "fs environment perturb did not reach the dither"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "fs perturb short and environment output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs" \
    --precision-policy=8bit "${input_image}" >"${default_output}" || {
    echo "not ok" 1 - "fs default 8-bit conversion failed"
    exit 0
}

cmp -s "${short_output}" "${default_output}" && {
    echo "not ok" 1 - "fs perturb did not affect 8-bit output"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs:U0.5" \
    --precision-policy=float32 "${input_image}" >"${float_short_output}" || {
    echo "not ok" 1 - "fs float32 short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_PERTURB=0.5" -d "fs" \
    --precision-policy=float32 "${input_image}" >"${float_env_output}" || {
    echo "not ok" 1 - "fs float32 environment conversion failed"
    exit 0
}

cmp -s "${float_short_output}" "${float_env_output}" || {
    echo "not ok" 1 - "fs float32 short and environment output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs" \
    --precision-policy=float32 "${input_image}" >"${float_default_output}" || {
    echo "not ok" 1 - "fs default float32 conversion failed"
    exit 0
}

cmp -s "${float_short_output}" "${float_default_output}" && {
    echo "not ok" 1 - "fs perturb did not affect float32 output"
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

echo "ok" 1 - "fs:perturb preserves effective image output"
exit 0
