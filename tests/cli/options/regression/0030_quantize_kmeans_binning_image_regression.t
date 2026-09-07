#!/bin/sh
# Verify shared quantizer binning through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|NULL|binning_policy
# Registry binding: palette_binning_policy|palette_binning_override

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0030-quantize-binning-short-$$.six"
env_output="${artifact_dir}/0030-quantize-binning-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qauto:Wsoft \
    -p 16 -Qkmeans "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "short soft binning conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=binning_policy|stored=1|binding=palette_binning_policy,palette_binning_override|value=4*}" != "${short_trace}" || {
    echo "not ok" 1 - "short soft binning value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_BINNING_POLICY=soft" -p 16 -Qkmeans \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "environment soft binning conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=binning_policy|stored=1|binding=palette_binning_policy,palette_binning_override|value=4*}" != "${env_trace}" || {
    echo "not ok" 1 - "environment soft binning value was not stored"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "short and environment soft binning output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "soft binning image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "shared quantizer binning preserves image output"
exit 0
