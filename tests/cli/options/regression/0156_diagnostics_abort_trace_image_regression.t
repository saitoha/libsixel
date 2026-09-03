#!/bin/sh
# Verify abort trace policy through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|abort_trace
# Registry binding: abort_trace|abort_trace_override
# Abort trace contract: enabled=0|installed=0

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
short_output="${artifact_dir}/0156-abort-trace-short-$$.six"
env_output="${artifact_dir}/0156-abort-trace-env-$$.six"

short_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_TRACE_TOPIC=suboption_contract,aborttrace_contract" \
    --env "SIXEL_ABORT_TRACE=1" "-xhuman:A0" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "abort trace short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=abort_trace|stored=1|binding=abort_trace,abort_trace_override|value=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "abort trace short value was not stored"
    exit 0
}
test "${short_trace#*LSXABT1|*enabled=0|installed=0*}" != \
    "${short_trace}" || {
    echo "not ok" 1 - "abort trace short value missed its consumer"
    exit 0
}

env_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_TRACE_TOPIC=suboption_contract,aborttrace_contract" \
    --env "SIXEL_ABORT_TRACE=0" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "abort trace environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=abort_trace|stored=1|binding=abort_trace,abort_trace_override|value=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "abort trace environment value was not stored"
    exit 0
}
test "${env_trace#*LSXABT1|*enabled=0|installed=0*}" != \
    "${env_trace}" || {
    echo "not ok" 1 - "abort trace environment value missed its consumer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "abort trace outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "abort trace image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "abort trace preserves image and install policy"
exit 0
