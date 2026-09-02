#!/bin/sh
# Verify center:auto_fft_threshold preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER|auto_fft_threshold
# Registry binding: quantize_model_kcenter_auto_fft_threshold|quantize_model_kcenter_auto_fft_threshold_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0066-quantize-center-auto_fft_threshold-short-$$.six"
env_output="${artifact_dir}/0066-quantize-center-auto_fft_threshold-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 64 "-Qcenter:Aauto:Qadaptive:F256" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "center:auto_fft_threshold short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=auto_fft_threshold|stored=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "auto_fft_threshold short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_AUTO_FFT_THRESHOLD=256" -p 64 "-Qcenter:Aauto:Qadaptive" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "center:auto_fft_threshold env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=auto_fft_threshold|stored=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "auto_fft_threshold environment value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "center:auto_fft_threshold short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "center:auto_fft_threshold image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "center:auto_fft_threshold preserves image output"
exit 0
