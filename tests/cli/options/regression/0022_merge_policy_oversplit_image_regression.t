#!/bin/sh
# Verify merge-policy oversplit preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_MERGE_POLICY|NULL|merge_oversplit
# Registry binding: merge_policy_oversplit|merge_policy_oversplit_override
# Environment range: clamp-both
# Palette contract: oversplit=3

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
short_output="${artifact_dir}/0022-quantize-heckbert-merge_oversplit-short-$$.six"
env_output="${artifact_dir}/0022-quantize-heckbert-merge_oversplit-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 -Qheckbert "-Fward:O3" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "merge_oversplit short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=merge_oversplit|stored=1|binding=merge_policy_oversplit,merge_policy_oversplit_override|value=3*}" != "${short_trace}" || {
    echo "not ok" 1 - "merge_oversplit short value was not stored"
    exit 0
}

test "${short_trace#*LSXMRG1|*oversplit=3*}" != "${short_trace}" || {
    echo "not ok" 1 - "merge_oversplit missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_OVERSPLIT_FACTOR=4" -p 16 -Qheckbert -Fward \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "merge_oversplit env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=merge_oversplit|stored=1|binding=merge_policy_oversplit,merge_policy_oversplit_override|value=3*}" != "${env_trace}" || {
    echo "not ok" 1 - "merge_oversplit environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_OVERSPLIT_FACTOR=0" \
    -p 16 -Qheckbert -Fward "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "merge_oversplit lower conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1|*key=merge_oversplit|stored=1|binding=merge_policy_oversplit,merge_policy_oversplit_override|value=1*}" != "${range_trace}" || {
    echo "not ok" 1 - "merge_oversplit lower endpoint was not clamped"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "merge-policy oversplit short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "merge-policy oversplit image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "merge-policy oversplit preserves image output"
exit 0
