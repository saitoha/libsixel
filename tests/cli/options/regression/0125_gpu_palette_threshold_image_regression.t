#!/bin/sh
# Verify encoder GPU threshold through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_GPU_POLICY|NULL|palette_threshold
# Registry binding: gpu_palette_threshold|gpu_palette_threshold_override
# Dither contract: gpu_threshold=300|gpu_threshold_override=1
# Environment range: saturate-unsigned-long

# Image assertion: option-values-and-output-equivalence
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0125-gpu-palette-threshold-short-$$.six"
env_output="${artifact_dir}/0125-gpu-palette-threshold-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -G "auto:P300" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "GPU palette threshold short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=palette_threshold\|stored=1\|binding=gpu_palette_threshold,gpu_palette_threshold_override\|value=300**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "GPU palette threshold short value was not stored"
    exit 0
}

case "${short_trace}" in *LSXDTH1\|*gpu_threshold=300\|gpu_threshold_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "GPU palette threshold did not reach the dither"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_GPU_PALETTE_THRESHOLD=300" -G auto \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "GPU palette threshold env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=palette_threshold\|stored=1\|binding=gpu_palette_threshold,gpu_palette_threshold_override\|value=300**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "GPU palette threshold environment value was not stored"
    exit 0
}

case "${env_trace}" in *LSXDTH1\|*gpu_threshold=300\|gpu_threshold_override=1**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "environment GPU threshold did not reach the dither"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "GPU palette threshold outputs differ"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_GPU_PALETTE_THRESHOLD=184467440737095516150" \
    -G off "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "GPU palette threshold overflow conversion failed"
    exit 0
}

case "${range_trace}" in *LSXSUB1\|*key=palette_threshold\|stored=1\|binding=gpu_palette_threshold,gpu_palette_threshold_override\|value=*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "GPU palette threshold did not saturate overflow"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Image quality belongs to the dither, palette and loader quality suites.
test -s "${short_output}" || {
    echo "not ok 1 - option output is empty"
    exit 0
}

echo "ok" 1 - "GPU palette threshold preserves image output"
exit 0
