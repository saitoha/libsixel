#!/bin/sh
# Verify stbn:scene_detect preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN|scene_detect
# Registry binding: stbn_scene_detect_enabled|stbn_scene_detect_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
animated_image="${TOP_SRCDIR}/tests/data/inputs/snake_motion_64_2frame.gif"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0011-diffusion-stbn-scene_detect-short-$$.six"
env_output="${artifact_dir}/0011-diffusion-stbn-scene_detect-env-$$.six"

effect_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L builtin -ldisable -p 64 \
    -d "stbn:Spmj:E1" "${animated_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "diffusion stbn scene detect animated conversion failed"
    exit 0
}

test "${effect_trace#*LSXDTH1|*consume=[1-9]*}" != "${effect_trace}" || {
    echo "not ok" 1 - "diffusion stbn scene detect did not consume a second frame"
    exit 0
}

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L builtin -ldisable -p 64 \
    -d "stbn:Spmj:E1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "stbn:scene_detect short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=scene_detect|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "scene_detect short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_STBN_SCENE_DETECT=1" \
    --threads=1 -L builtin -ldisable -p 64 \
    -d "stbn:Spmj" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "stbn:scene_detect env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=scene_detect|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "scene_detect environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "stbn:scene_detect short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "stbn:scene_detect image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "stbn:scene_detect preserves image output"
exit 0
