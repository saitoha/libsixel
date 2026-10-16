#!/bin/sh
# Exercise runtime sampling threshold routing on the adaptive-grid path.
# Verify runtime sample threshold through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_RUNTIME_POLICY|NULL|sample_min
# Registry binding: sample_parallel_min_pixels|sample_parallel_min_pixels_override
# Environment range: clamp-size-width
# Runtime contract: trace=sample_min=257

# Image assertion: option-values-and-output-equivalence
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0184-sample-min-short-$$.six"
env_output="${artifact_dir}/0184-sample-min-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -4adaptive-grid \
    "-jauto:S257" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "sample threshold short conversion failed"
    exit 0
}
case "${short_trace}" in *LSXSUB1\|*key=sample_min\|stored=1\|binding=sample_parallel_min_pixels,sample_parallel_min_pixels_override\|value=257**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "sample threshold short value was not stored"
    exit 0
}
case "${short_trace}" in *LSXRT1\|sample_min=257**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "sample threshold missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -4adaptive-grid \
    --env "SIXEL_SAMPLE_PARALLEL_MIN_PIXELS=257" -jauto \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "sample threshold environment conversion failed"
    exit 0
}
case "${env_trace}" in *LSXSUB1\|*key=sample_min\|stored=1\|binding=sample_parallel_min_pixels,sample_parallel_min_pixels_override\|value=257**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "sample threshold environment value was not stored"
    exit 0
}
case "${env_trace}" in *LSXRT1\|sample_min=257**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "environment threshold missed its consumer"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -4adaptive-grid \
    --env "SIXEL_SAMPLE_PARALLEL_MIN_PIXELS=-1" -jauto \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "sample threshold signed conversion failed"
    exit 0
}
case "${range_trace}" in *LSXSUB1\|*key=sample_min\|stored=1\|binding=sample_parallel_min_pixels,sample_parallel_min_pixels_override\|value=**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "sample threshold did not preserve size clamping"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "sample threshold outputs differ"
    exit 0
}
# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "runtime sample threshold preserves image output"
exit 0
