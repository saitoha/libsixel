#!/bin/sh
# Policy: docs/functionality/cover-policy.md
# Verify cover-policy grow preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_COVER_POLICY|NULL|cover_grow
# Registry binding: cover_policy_grow|cover_policy_grow_override
# Palette contract: grow=1

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
short_output="${artifact_dir}/0025-quantize-heckbert-cover_grow-short-$$.six"
env_output="${artifact_dir}/0025-quantize-heckbert-cover_grow-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 -Qheckbert "-acorners:V1" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "cover_grow short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=cover_grow|stored=1|binding=cover_policy_grow,cover_policy_grow_override|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "cover_grow short value was not stored"
    exit 0
}

test "${short_trace#*LSXCOV1|*grow=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "cover_grow missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_COVER_GROW=1" -p 16 -Qheckbert -acorners \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "cover_grow env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=cover_grow|stored=1|binding=cover_policy_grow,cover_policy_grow_override|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "cover_grow environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "cover-policy grow short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "cover-policy grow image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "cover-policy grow preserves image output"
exit 0
