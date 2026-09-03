#!/bin/sh
# Verify PSD trace-only control through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|psd_trace
# Registry binding: psd_trace|psd_trace_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_layers_minimal_type_layer.psd"
reference_image="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psdtools_layers_minimal_type_layer_expected_psdtools.ppm"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0152-psd-trace-short-$$.six"
env_output="${artifact_dir}/0152-psd-trace-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,psd_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PSD_TRACE_ONLY=1" "-xhuman:D0" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "PSD trace short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=psd_trace|stored=1|binding=psd_trace,psd_trace_override|value=0*}" != "${short_trace}" || {
    echo "not ok" 1 - "PSD trace short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,psd_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PSD_TRACE_ONLY=0" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "PSD trace environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=psd_trace|stored=1|binding=psd_trace,psd_trace_override|value=0*}" != "${env_trace}" || {
    echo "not ok" 1 - "PSD trace environment value was not stored"
    exit 0
}

test -s "${short_output}" || {
    echo "not ok" 1 - "PSD trace disable did not reach the encoder"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "PSD trace outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "PSD trace image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "PSD trace control preserves decoded image output"
exit 0
