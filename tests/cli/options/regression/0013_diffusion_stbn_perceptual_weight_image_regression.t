#!/bin/sh
# Verify stbn:perceptual_weight preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN|perceptual_weight

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_ROOT}/suboption-regression"
test -d "${artifact_dir}" || mkdir -p "${artifact_dir}"
short_output="${artifact_dir}/0013-diffusion-stbn-perceptual_weight-short.six"
env_output="${artifact_dir}/0013-diffusion-stbn-perceptual_weight-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d "stbn:P0" "${input_image}" -o "${short_output}" || {
    echo "not ok" 1 - "stbn:perceptual_weight short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT=0" -d "stbn" \
    "${input_image}" -o "${env_output}" || {
    echo "not ok" 1 - "stbn:perceptual_weight env conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "stbn:perceptual_weight short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "stbn:perceptual_weight image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "stbn:perceptual_weight preserves image output"
exit 0
