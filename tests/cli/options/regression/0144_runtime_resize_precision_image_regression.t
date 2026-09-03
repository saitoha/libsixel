#!/bin/sh
# Verify runtime resize precision through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_RUNTIME_POLICY|NULL|resize_precision
# Registry binding: resize_precision|resize_precision_override
# Environment range: parse-signed-choice-prefix
# Runtime contract: trace=resize_precision=3

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
short_output="${artifact_dir}/0144-resize-precision-short-$$.six"
env_output="${artifact_dir}/0144-resize-precision-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -w16 \
    "-jauto:Rfloat" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "resize precision short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=resize_precision|stored=1|binding=resize_precision,resize_precision_override|value=3*}" != "${short_trace}" || {
    echo "not ok" 1 - "resize precision short value was not stored"
    exit 0
}
test "${short_trace#*LSXRT1|resize_precision=3*}" != "${short_trace}" || {
    echo "not ok" 1 - "resize precision missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -w16 \
    --env "SIXEL_PLANNER_RESIZE_PRECISION_MODE=3" -jauto \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "resize precision environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=resize_precision|stored=1|binding=resize_precision,resize_precision_override|value=3*}" != "${env_trace}" || {
    echo "not ok" 1 - "resize precision environment value was not stored"
    exit 0
}
test "${env_trace#*LSXRT1|resize_precision=3*}" != "${env_trace}" || {
    echo "not ok" 1 - "environment precision missed its consumer"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -w16 \
    --env "SIXEL_PLANNER_RESIZE_PRECISION_MODE=2junk" -jauto \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "resize precision prefix conversion failed"
    exit 0
}
test "${range_trace#*LSXSUB1|*key=resize_precision|stored=1|binding=resize_precision,resize_precision_override|value=2*}" != "${range_trace}" || {
    echo "not ok" 1 - "resize precision lost signed-prefix parsing"
    exit 0
}
test "${range_trace#*LSXRT1|resize_precision=2*}" != "${range_trace}" || {
    echo "not ok" 1 - "prefixed resize precision missed its consumer"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "resize precision outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "resize precision image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "runtime resize precision preserves image output"
exit 0
