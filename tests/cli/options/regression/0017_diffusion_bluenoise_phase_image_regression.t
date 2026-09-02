#!/bin/sh
# Verify bluenoise:phase preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE|phase
# Registry binding: bluenoise_phase_x|bluenoise_phase_y|bluenoise_phase_override

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
short_output="${artifact_dir}/0017-diffusion-bluenoise-phase-short-$$.six"
env_output="${artifact_dir}/0017-diffusion-bluenoise-phase-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d "bluenoise:P7,13" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "bluenoise:phase short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=phase|stored=1|binding=bluenoise_phase_x,bluenoise_phase_y,bluenoise_phase_override*}" != "${short_trace}" || {
    echo "not ok" 1 - "phase short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_BLUENOISE_PHASE=7,13" -d "bluenoise" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "bluenoise:phase env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=phase|stored=1|binding=bluenoise_phase_x,bluenoise_phase_y,bluenoise_phase_override*}" != "${env_trace}" || {
    echo "not ok" 1 - "phase environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "bluenoise:phase short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "bluenoise:phase image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "bluenoise:phase preserves image output"
exit 0
