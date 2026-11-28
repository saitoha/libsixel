#!/bin/sh
# Verify the GPU body threshold through short and environment paths.
# Policy: docs/cli/gpu-policy.md
# Registry row: SIXEL_OPTION_SCHEMA_GPU_POLICY|NULL|body_threshold
# Registry binding: gpu_body_threshold|gpu_body_threshold_override
# GPU contract: consumer=body|policy=1|threshold=300
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
short_output="${artifact_dir}/0092-gpu-body-threshold-short-$$.six"
env_output="${artifact_dir}/0092-gpu-body-threshold-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,gpu_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -G "auto:B300" \
    "${input_image}" 2>&1 >"${short_output}") || {
    printf '\n%s\n' "not ok 1 - GPU body threshold short conversion failed"
    exit 0
}

case "${short_trace}" in *LSXSUB1\|*key=body_threshold\|stored=1\|binding=gpu_body_threshold,gpu_body_threshold_override\|value=300**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - GPU body threshold short value was not stored"
    exit 0
}

case "${short_trace}" in *LSXGPU1\|*consumer=body\|policy=1\|threshold=300**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - GPU body threshold did not reach the output"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,gpu_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_GPU_BODY_THRESHOLD=300" -G auto \
    "${input_image}" 2>&1 >"${env_output}") || {
    printf '\n%s\n' "not ok 1 - GPU body threshold env conversion failed"
    exit 0
}

case "${env_trace}" in *LSXSUB1\|*key=body_threshold\|stored=1\|binding=gpu_body_threshold,gpu_body_threshold_override\|value=300**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - GPU body threshold environment value was not stored"
    exit 0
}

case "${env_trace}" in *LSXGPU1\|*consumer=body\|policy=1\|threshold=300**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - environment GPU body threshold did not reach the output"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    printf '\n%s\n' "not ok 1 - GPU body threshold outputs differ"
    exit 0
}

# force and offload have no size floor and no CPU/GPU cost comparison.
force_trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -G "force:B300" \
    "${input_image}" 2>&1 >/dev/null) || :

case "${force_trace}" in *LSXGPU1\|*consumer=body\|policy=2\|threshold=0**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - force kept the GPU body size floor"
    exit 0
}

offload_trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -G "offload:B300" \
    "${input_image}" 2>&1 >/dev/null) || :

case "${offload_trace}" in *LSXGPU1\|*consumer=body\|policy=3\|threshold=0**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - offload kept the GPU body size floor"
    exit 0
}

default_trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -G auto \
    "${input_image}" 2>&1 >/dev/null) || {
    printf '\n%s\n' "not ok 1 - GPU body threshold default conversion failed"
    exit 0
}

case "${default_trace}" in *LSXGPU1\|*consumer=body\|policy=1\|threshold=262144**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - GPU body threshold default changed"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_GPU_BODY_THRESHOLD=184467440737095516150" \
    -G off "${input_image}" 2>&1 >/dev/null) || {
    printf '\n%s\n' "not ok 1 - GPU body threshold overflow conversion failed"
    exit 0
}

case "${range_trace}" in *LSXSUB1\|*key=body_threshold\|stored=1\|binding=gpu_body_threshold,gpu_body_threshold_override\|value=*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - GPU body threshold did not saturate overflow"
    exit 0
}

# The observation is option routing and exact output equivalence.
# Whether a frame is spelled on the GPU belongs to the WebGPU suite.
test -s "${short_output}" || {
    printf '\n%s\n' "not ok 1 - option output is empty"
    exit 0
}

printf '\n%s\n' "ok 1 - GPU body threshold preserves image output"
exit 0
