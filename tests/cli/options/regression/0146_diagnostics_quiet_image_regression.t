#!/bin/sh
# Verify diagnostics quiet mode through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|quiet
# Registry binding: quiet|quiet_override

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
short_output="${artifact_dir}/0146-diagnostics-quiet-short-$$.six"
env_output="${artifact_dir}/0146-diagnostics-quiet-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DIAG_MODE_QUIET=0" "-xhuman:Q1" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "diagnostics quiet short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=quiet|stored=1|binding=quiet,quiet_override|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "diagnostics quiet short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DIAG_MODE_QUIET=1" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "diagnostics quiet environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=quiet|stored=1|binding=quiet,quiet_override|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "diagnostics quiet environment value was not stored"
    exit 0
}

status=0
message=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_DIAG_MODE_QUIET=0" \
    -xcode:Q1 -d invalidvalue "${input_image}" \
    -o/dev/null 2>&1) || status=$?
test "${status}" -eq 2 || {
    echo "not ok" 1 - "diagnostics quiet consumer exit status mismatch"
    exit 0
}
test "${message#*-d DIFFUSION,*}" = "${message}" || {
    echo "not ok" 1 - "diagnostics quiet did not suppress option help"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "diagnostics quiet outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "diagnostics quiet image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "diagnostics quiet preserves image and diagnostic output"
exit 0
