#!/bin/sh
# Verify stbn:strength preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN|strength
# Registry binding: interframe_noise_strength_u8|interframe_noise_strength_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
animated_image="${TOP_SRCDIR}/tests/data/inputs/snake_motion_64_2frame.gif"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0008-diffusion-stbn-strength-short-$$.six"
env_output="${artifact_dir}/0008-diffusion-stbn-strength-env-$$.six"

effect_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L builtin -ldisable -p 128 \
    -d "stbn:Spmj:T0.06" "${animated_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "diffusion stbn strength animated conversion failed"
    exit 0
}

test "${effect_trace#*LSXDTH1|*consume=[1-9]*}" != "${effect_trace}" || {
    echo "not ok" 1 - "diffusion stbn strength did not consume a second frame"
    exit 0
}

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L builtin -ldisable -p 128 \
    -d "stbn:Spmj:T0.06" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "stbn:strength short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=strength|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "strength short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_STBN_STRENGTH=0.06" \
    --threads=1 -L builtin -ldisable -p 128 \
    -d "stbn:Spmj" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "stbn:strength env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=strength|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "strength environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "stbn:strength short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "stbn:strength image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "stbn:strength preserves image output"
exit 0
