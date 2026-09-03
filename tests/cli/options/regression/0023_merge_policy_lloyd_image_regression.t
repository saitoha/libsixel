#!/bin/sh
# Verify merge-policy Lloyd passes preserve output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_MERGE_POLICY|NULL|merge_lloyd
# Registry binding: merge_policy_lloyd|merge_policy_lloyd_override
# Environment range: clamp-signed-uint
# Palette contract: lloyd=0

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
short_output="${artifact_dir}/0023-quantize-heckbert-merge_lloyd-short-$$.six"
env_output="${artifact_dir}/0023-quantize-heckbert-merge_lloyd-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 -Qheckbert "-Fward:L0" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "merge_lloyd short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=merge_lloyd|stored=1|binding=merge_policy_lloyd,merge_policy_lloyd_override|value=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "merge_lloyd short value was not stored"
    exit 0
}

test "${short_trace#*LSXMRG1|*lloyd=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "merge_lloyd missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=-1" \
    -p 16 -Qheckbert -Fward \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "merge_lloyd env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=merge_lloyd|stored=1|binding=merge_policy_lloyd,merge_policy_lloyd_override|value=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "merge_lloyd environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=31" \
    -p 16 -Qheckbert -Fward "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "merge_lloyd upper conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1|*key=merge_lloyd|stored=1|binding=merge_policy_lloyd,merge_policy_lloyd_override|value=30*}" != "${range_trace}" || {
    echo "not ok" 1 - "merge_lloyd upper endpoint was not clamped"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "merge-policy Lloyd short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "merge-policy Lloyd image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "merge-policy Lloyd passes preserve image output"
exit 0
