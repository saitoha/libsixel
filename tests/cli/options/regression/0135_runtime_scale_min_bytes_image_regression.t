#!/bin/sh
# Verify runtime scale threshold through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_RUNTIME_POLICY|NULL|scale_min_bytes
# Registry binding: scale_parallel_min_bytes|scale_parallel_min_bytes_override
# Environment range: clamp-size-width
# Runtime contract: runner=scale/0002_parallel_min_bytes_environment|mode=cli,negative

# Image assertion: option-values-and-output-equivalence
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
short_output="${artifact_dir}/0145-scale-min-bytes-short-$$.six"
env_output="${artifact_dir}/0145-scale-min-bytes-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=balanced:B2 --lookup-policy=none \
    "-jauto:B17" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "scale threshold short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=scale_min_bytes\|stored=1\|binding=scale_parallel_min_bytes,scale_parallel_min_bytes_override\|value=17**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "scale threshold short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=balanced:B2 --lookup-policy=none \
    --env "SIXEL_SCALE_PARALLEL_MIN_BYTES=17" -jauto \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "scale threshold environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=scale_min_bytes\|stored=1\|binding=scale_parallel_min_bytes,scale_parallel_min_bytes_override\|value=17**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "scale threshold environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -=balanced:B2 \
    --env "SIXEL_SCALE_PARALLEL_MIN_BYTES=-1" -jauto \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "scale threshold signed conversion failed"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=scale_min_bytes\|stored=1\|binding=scale_parallel_min_bytes,scale_parallel_min_bytes_override\|value=**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "scale threshold did not preserve size clamping"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "scale/0002_parallel_min_bytes_environment" cli || {
    echo "not ok" 1 - "scale threshold CLI missed parallel dispatch"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "scale/0002_parallel_min_bytes_environment" negative || {
    echo "not ok" 1 - "scale threshold lost signed strtoull behavior"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "scale threshold outputs differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "runtime scale threshold preserves image output"
exit 0
