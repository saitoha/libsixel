#!/bin/sh
# Verify runtime decoder skew through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_RUNTIME_POLICY|NULL|parallel_skew
# Registry binding: parallel_skew|parallel_skew_override
# Environment range: clamp-both
# Runtime contract: runner=decoder/0024_decoder_parallel_skew_environment|mode=cli

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/images/map8.six"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0143-parallel-skew-short-$$.png"
env_output="${artifact_dir}/0143-parallel-skew-env-$$.png"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D \
    "-jauto:K20" <"${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "parallel skew short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=parallel_skew|stored=1|binding=parallel_skew,parallel_skew_override|value=20*}" != "${short_trace}" || {
    echo "not ok" 1 - "parallel skew short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D \
    --env "SIXEL_PARALLEL_SKEW=20" -jauto \
    <"${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "parallel skew environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=parallel_skew|stored=1|binding=parallel_skew,parallel_skew_override|value=20*}" != "${env_trace}" || {
    echo "not ok" 1 - "parallel skew environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D \
    --env "SIXEL_PARALLEL_SKEW=-21" -jauto \
    <"${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "parallel skew lower conversion failed"
    exit 0
}
test "${range_trace#*LSXSUB1|*key=parallel_skew|stored=1|binding=parallel_skew,parallel_skew_override|value=-20*}" != "${range_trace}" || {
    echo "not ok" 1 - "parallel skew did not clamp its lower endpoint"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0024_decoder_parallel_skew_environment" cli || {
    echo "not ok" 1 - "parallel skew CLI missed span planning"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "parallel skew outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "parallel skew image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "runtime parallel skew preserves image output"
exit 0
