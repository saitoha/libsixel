#!/bin/sh
# Verify cover-policy mode preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_COVER_POLICY|NULL|cover_mode
# Registry binding: cover_policy_mode|cover_policy_mode_override
# Palette contract: mode=hard

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0026-quantize-heckbert-cover_mode-short-$$.six"
env_output="${artifact_dir}/0026-quantize-heckbert-cover_mode-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 -Qheckbert "-acorners:Whard" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "cover_mode short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=cover_mode|stored=1|binding=cover_policy_mode,cover_policy_mode_override|value=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "cover_mode short value was not stored"
    exit 0
}

test "${short_trace#*LSXCOV1|*mode=hard*}" != "${short_trace}" || {
    echo "not ok" 1 - "cover_mode missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_COVER_MODE=hard" -p 16 -Qheckbert -acorners \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "cover_mode env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=cover_mode|stored=1|binding=cover_policy_mode,cover_policy_mode_override|value=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "cover_mode environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "cover-policy mode short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "cover-policy mode image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "cover-policy mode preserves image output"
exit 0
