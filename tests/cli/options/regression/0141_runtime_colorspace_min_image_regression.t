#!/bin/sh
# Verify runtime colorspace threshold through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_RUNTIME_POLICY|NULL|colorspace_min
# Registry binding: colorspace_parallel_min_pixels|colorspace_parallel_min_pixels_override
# Environment range: clamp-size-width
# Runtime contract: trace=colorspace_min=257

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
short_output="${artifact_dir}/0141-colorspace-min-short-$$.six"
env_output="${artifact_dir}/0141-colorspace-min-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Woklab \
    "-jauto:C257" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "colorspace threshold short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=colorspace_min|stored=1|binding=colorspace_parallel_min_pixels,colorspace_parallel_min_pixels_override|value=257*}" != "${short_trace}" || {
    echo "not ok" 1 - "colorspace threshold short value was not stored"
    exit 0
}
test "${short_trace#*LSXRT1|colorspace_min=257*}" != "${short_trace}" || {
    echo "not ok" 1 - "colorspace threshold missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,runtime_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Woklab \
    --env "SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=257" -jauto \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "colorspace threshold environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=colorspace_min|stored=1|binding=colorspace_parallel_min_pixels,colorspace_parallel_min_pixels_override|value=257*}" != "${env_trace}" || {
    echo "not ok" 1 - "colorspace threshold environment value was not stored"
    exit 0
}
test "${env_trace#*LSXRT1|colorspace_min=257*}" != "${env_trace}" || {
    echo "not ok" 1 - "environment threshold missed its consumer"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Woklab \
    --env "SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=-1" -jauto \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "colorspace threshold signed conversion failed"
    exit 0
}
test "${range_trace#*LSXSUB1|*key=colorspace_min|stored=1|binding=colorspace_parallel_min_pixels,colorspace_parallel_min_pixels_override|value=*}" != "${range_trace}" || {
    echo "not ok" 1 - "colorspace threshold did not preserve size clamping"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "colorspace threshold outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "colorspace threshold image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "runtime colorspace threshold preserves image output"
exit 0
