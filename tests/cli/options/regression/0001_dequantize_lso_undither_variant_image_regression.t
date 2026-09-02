#!/bin/sh
# Verify lso_undither:variant preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_DEQUANTIZE|g_dequantize_values + SIXEL_DEQUANTIZE_BASE_LSO_UNDITHER|variant

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/images/map8.six"
reference_image="${TOP_SRCDIR}/images/map8.six"
artifact_dir="${ARTIFACT_ROOT}/suboption-regression"
test -d "${artifact_dir}" || mkdir -p "${artifact_dir}"
short_output="${artifact_dir}/0001-dequantize-lso_undither-variant-short.png"
env_output="${artifact_dir}/0001-dequantize-lso_undither-variant-env.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    "-dlso_undither:Vlight" <"${input_image}" >"${short_output}" || {
    echo "not ok" 1 - "lso_undither:variant short conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env "SIXEL_DEQUANTIZE_LSO_VARIANT=light" "-dlso_undither" \
    <"${input_image}" >"${env_output}" || {
    echo "not ok" 1 - "lso_undither:variant env conversion failed"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "lso_undither:variant short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "lso_undither:variant image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "lso_undither:variant preserves image output"
exit 0
