#!/bin/sh
# Verify runtime parallel factor through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_RUNTIME_POLICY|NULL|parallel_factor
# Registry binding: parallel_factor|parallel_factor_override
# Environment range: parse-signed-long
# Runtime contract: runner=scale/0001_parallel_factor_environment|mode=cli

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0142-parallel-factor-short-$$.six"
env_output="${artifact_dir}/0142-parallel-factor-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=2 \
    "-jauto:F3" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "parallel factor short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1\|*key=parallel_factor\|stored=1\|binding=parallel_factor,parallel_factor_override\|value=3*}" != "${short_trace}" || {
    echo "not ok" 1 - "parallel factor short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=2 \
    --env "SIXEL_PARALLEL_FACTOR=3" -jauto \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "parallel factor environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1\|*key=parallel_factor\|stored=1\|binding=parallel_factor,parallel_factor_override\|value=3*}" != "${env_trace}" || {
    echo "not ok" 1 - "parallel factor environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=2 \
    --env "SIXEL_PARALLEL_FACTOR=0" -jauto \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "parallel factor zero conversion failed"
    exit 0
}
test "${range_trace#*LSXSUB1\|*key=parallel_factor\|stored=1*}" = "${range_trace}" || {
    echo "not ok" 1 - "parallel factor accepted zero"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "scale/0001_parallel_factor_environment" cli || {
    echo "not ok" 1 - "parallel factor CLI missed band planning"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "parallel factor outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "parallel factor image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "runtime parallel factor preserves image output"
exit 0
