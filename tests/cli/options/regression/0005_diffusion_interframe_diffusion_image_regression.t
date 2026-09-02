#!/bin/sh
# Verify interframe:diffusion preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_INTERFRAME|diffusion

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
short_output="${artifact_dir}/0005-diffusion-interframe-diffusion-short.six"
env_output="${artifact_dir}/0005-diffusion-interframe-diffusion-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d "interframe:Datkinson" "${input_image}" -o "${short_output}" || {
    echo "not ok" 1 - "interframe:diffusion short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_INTERFRAME_DIFFUSION=atkinson" -d "interframe" \
    "${input_image}" -o "${env_output}" || {
    echo "not ok" 1 - "interframe:diffusion env conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "interframe:diffusion short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "interframe:diffusion image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "interframe:diffusion preserves image output"
exit 0
