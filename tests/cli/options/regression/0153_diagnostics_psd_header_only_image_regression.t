#!/bin/sh
# Verify PSD header-only tracing through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIAGNOSTICS|NULL|psd_header_only
# Registry binding: psd_header_only|psd_header_only_override

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
short_output="${artifact_dir}/0153-psd-header-short-$$.six"
env_output="${artifact_dir}/0153-psd-header-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,psd_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PSD_TRACE_HEADER_ONLY=0" "-xhuman:E1" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "PSD header-only short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1\|*key=psd_header_only\|stored=1\|binding=psd_header_only,psd_header_only_override\|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "PSD header-only short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,psd_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PSD_TRACE_HEADER_ONLY=1" -xhuman \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "PSD header-only environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1\|*key=psd_header_only\|stored=1\|binding=psd_header_only,psd_header_only_override\|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "PSD header-only environment value was not stored"
    exit 0
}

test "${short_trace#*LSXPSD1\|*}" != "${short_trace}" || {
    echo "not ok" 1 - "PSD header-only trace omitted its summary"
    exit 0
}
test "${short_trace#*libsixel?psd_decode?:*}" = "${short_trace}" || {
    echo "not ok" 1 - "PSD header-only trace retained verbose messages"
    exit 0
}
cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "PSD header-only outputs differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "PSD header-only image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "PSD header-only tracing preserves decoded image output"
exit 0
