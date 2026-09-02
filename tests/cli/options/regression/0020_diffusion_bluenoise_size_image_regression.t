#!/bin/sh
# Verify bluenoise:size preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE|size
# Registry binding: bluenoise_size|bluenoise_size_override

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
short_output="${artifact_dir}/0020-diffusion-bluenoise-size-short-$$.six"
env_output="${artifact_dir}/0020-diffusion-bluenoise-size-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d "bluenoise:Z64" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "bluenoise:size short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=size|stored=1|binding=bluenoise_size,bluenoise_size_override*}" != "${short_trace}" || {
    echo "not ok" 1 - "size short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_BLUENOISE_SIZE=64" -d "bluenoise" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "bluenoise:size env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=size|stored=1|binding=bluenoise_size,bluenoise_size_override*}" != "${env_trace}" || {
    echo "not ok" 1 - "size environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "bluenoise:size short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "bluenoise:size image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "bluenoise:size preserves image output"
exit 0
