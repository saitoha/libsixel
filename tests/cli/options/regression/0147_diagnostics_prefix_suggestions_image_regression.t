#!/bin/sh
# Verify prefix suggestions through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|prefix_suggestions
# Registry binding: prefix_suggestions|prefix_suggestions_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0147-diagnostics-prefix-short-$$.six"
env_output="${artifact_dir}/0147-diagnostics-prefix-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OPTION_PREFIX_SUGGESTIONS=1" "-xhuman:P0" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "prefix suggestions short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=prefix_suggestions|stored=1|binding=prefix_suggestions,prefix_suggestions_override|value=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "prefix suggestions short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OPTION_PREFIX_SUGGESTIONS=0" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "prefix suggestions environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=prefix_suggestions|stored=1|binding=prefix_suggestions,prefix_suggestions_override|value=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "prefix suggestions environment value was not stored"
    exit 0
}

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OPTION_PREFIX_SUGGESTIONS=1" \
    -xhuman:P0 -d st "${input_image}" -o/dev/null 2>&1) || status=$?
test "${status}" -eq 2 || {
    echo "not ok" 1 - "prefix suggestions consumer exit status mismatch"
    exit 0
}
test "${message#*\(matches:*}" = "${message}" || {
    echo "not ok" 1 - "prefix suggestions remained enabled"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "prefix suggestions outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "prefix suggestions image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "prefix suggestions preserve image and diagnostic output"
exit 0
