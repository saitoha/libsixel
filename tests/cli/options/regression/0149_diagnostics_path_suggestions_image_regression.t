#!/bin/sh
# Verify path suggestions through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|path_suggestions
# Registry binding: path_suggestions|path_suggestions_override

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
short_output="${artifact_dir}/0149-diagnostics-path-short-$$.six"
env_output="${artifact_dir}/0149-diagnostics-path-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OPTION_PATH_SUGGESTIONS=0" "-xhuman:S1" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "path suggestions short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=path_suggestions|stored=1|binding=path_suggestions,path_suggestions_override|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "path suggestions short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OPTION_PATH_SUGGESTIONS=1" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "path suggestions environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=path_suggestions|stored=1|binding=path_suggestions,path_suggestions_override|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "path suggestions environment value was not stored"
    exit 0
}

message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_OPTION_PATH_SUGGESTIONS=0" \
    -xhuman:S1 "${TOP_SRCDIR}/tests/data/inputs/xxxxx.xxx" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "path suggestions accepted a missing input"
    exit 0
}
suggestion_found=0
test "${message#*Suggestion*}" != "${message}" && suggestion_found=1
test "${message#*No nearby matches*}" != "${message}" && suggestion_found=1
test "${suggestion_found}" -eq 1 || {
    echo "not ok" 1 - "path suggestions did not reach consumer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "path suggestions outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "path suggestions image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "path suggestions preserve image and diagnostic output"
exit 0
