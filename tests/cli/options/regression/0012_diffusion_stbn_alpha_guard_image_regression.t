#!/bin/sh
# Verify stbn:alpha_guard preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN|alpha_guard
# Registry binding: stbn_alpha_guard_enabled|stbn_alpha_guard_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgba.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgba.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0012-diffusion-stbn-alpha_guard-short-$$.six"
env_output="${artifact_dir}/0012-diffusion-stbn-alpha_guard-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -L builtin -ldisable -p 64 \
    -d "stbn:Smask:A1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "stbn:alpha_guard short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=alpha_guard|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "alpha_guard short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DITHER_STBN_ALPHA_GUARD=1" \
    --threads=1 -L builtin -ldisable -p 64 \
    -d "stbn:Smask" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "stbn:alpha_guard env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=alpha_guard|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "alpha_guard environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "stbn:alpha_guard short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "stbn:alpha_guard image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "stbn:alpha_guard preserves image output"
exit 0
