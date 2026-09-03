#!/bin/sh
# Verify decoder GPU threshold through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_GPU_POLICY|NULL|dequant_threshold
# Registry binding: gpu_dequant_threshold|gpu_dequant_threshold_override
# GPU contract: consumer=decoder|policy=1|threshold=300
# Environment range: saturate-unsigned-long

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/images/map8.six"
reference_image="${input_image}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0126-gpu-dequant-threshold-short-$$.png"
env_output="${artifact_dir}/0126-gpu-dequant-threshold-env-$$.png"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,gpu_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D -d lso_undither:Vlight \
    -G "auto:D300" <"${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "GPU dequant threshold short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=dequant_threshold|stored=1|binding=gpu_dequant_threshold,gpu_dequant_threshold_override|value=300*}" != "${short_trace}" || {
    echo "not ok" 1 - "GPU dequant threshold short value was not stored"
    exit 0
}

test "${short_trace#*LSXGPU1|*consumer=decoder|policy=1|threshold=300*}" != "${short_trace}" || {
    echo "not ok" 1 - "GPU dequant threshold did not reach the request"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,gpu_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D -d lso_undither:Vlight \
    --env "SIXEL_GPU_DEQUANT_THRESHOLD=300" -G auto \
    <"${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "GPU dequant threshold env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=dequant_threshold|stored=1|binding=gpu_dequant_threshold,gpu_dequant_threshold_override|value=300*}" != "${env_trace}" || {
    echo "not ok" 1 - "GPU dequant threshold environment value was not stored"
    exit 0
}

test "${env_trace#*LSXGPU1|*consumer=decoder|policy=1|threshold=300*}" != "${env_trace}" || {
    echo "not ok" 1 - "environment GPU threshold did not reach the request"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "GPU dequant threshold outputs differ"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D -d lso_undither:Vlight \
    --env "SIXEL_GPU_DEQUANT_THRESHOLD=184467440737095516150" -G off \
    <"${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "GPU dequant threshold overflow conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1|*key=dequant_threshold|stored=1|binding=gpu_dequant_threshold,gpu_dequant_threshold_override|value=}" != "${range_trace}" || {
    echo "not ok" 1 - "GPU dequant threshold did not saturate overflow"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "GPU dequant threshold image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "GPU dequant threshold preserves image output"
exit 0
