#!/bin/sh
# Verify fs:perturb_seed through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|NULL|perturb_seed
# Registry binding: diffusion_perturb_seed|diffusion_perturb_seed_override
# Dither contract: diffuse=fs|*perturb_seed=1|perturb_seed_override=1

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
short_output="${artifact_dir}/0167-perturb-seed-short-$$.six"
env_output="${artifact_dir}/0167-perturb-seed-env-$$.six"
default_output="${artifact_dir}/0167-perturb-seed-default-$$.six"
float_short_output="${artifact_dir}/0167-perturb-seed-float-short-$$.six"
float_env_output="${artifact_dir}/0167-perturb-seed-float-env-$$.six"
float_default_output="${artifact_dir}/0167-perturb-seed-float-default-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs:perturb=0.5:R1" \
    --precision=8bit "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "fs:perturb_seed short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=perturb_seed|stored=1|binding=diffusion_perturb_seed,diffusion_perturb_seed_override|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "fs perturb_seed short value was not stored"
    exit 0
}

test "${short_trace#*LSXDTH1|*diffuse=fs|*perturb_seed=1|perturb_seed_override=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "fs perturb_seed did not reach the dither"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_PERTURB_SEED=1" -d "fs:perturb=0.5" \
    --precision=8bit "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "fs:perturb_seed environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=perturb_seed|stored=1|binding=diffusion_perturb_seed,diffusion_perturb_seed_override|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "fs perturb_seed environment value was not stored"
    exit 0
}

test "${env_trace#*LSXDTH1|*diffuse=fs|*perturb_seed=1|perturb_seed_override=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "fs environment perturb_seed did not reach the dither"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "fs perturb_seed short and environment output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs:perturb=0.5" \
    --precision=8bit "${input_image}" >"${default_output}" || {
    echo "not ok" 1 - "fs default 8-bit conversion failed"
    exit 0
}

cmp -s "${short_output}" "${default_output}" && {
    echo "not ok" 1 - "fs perturb_seed did not affect 8-bit output"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs:perturb=0.5:R1" \
    --precision=float32 "${input_image}" >"${float_short_output}" || {
    echo "not ok" 1 - "fs float32 short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_PERTURB_SEED=1" -d "fs:perturb=0.5" \
    --precision=float32 "${input_image}" >"${float_env_output}" || {
    echo "not ok" 1 - "fs float32 environment conversion failed"
    exit 0
}

cmp -s "${float_short_output}" "${float_env_output}" || {
    echo "not ok" 1 - "fs float32 short and environment output differ"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d "fs:perturb=0.5" \
    --precision=float32 "${input_image}" >"${float_default_output}" || {
    echo "not ok" 1 - "fs default float32 conversion failed"
    exit 0
}

cmp -s "${float_short_output}" "${float_default_output}" && {
    echo "not ok" 1 - "fs perturb_seed did not affect float32 output"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "fs perturb_seed 8-bit image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

float_lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${float_short_output}" 2>&1) || float_lsqa_status=$?
test "${float_lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "fs perturb_seed float32 image quality regressed"
    printf "# %s\n" "${float_lsqa_error}"
    exit 0
}

echo "ok" 1 - "fs:perturb_seed preserves effective image output"
exit 0
