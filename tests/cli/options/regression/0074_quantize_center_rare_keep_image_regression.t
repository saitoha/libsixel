#!/bin/sh
# Verify center:rare_keep preserves image output through short and env paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER|rare_keep
# Registry binding: quantize_model_kcenter_rare_keep|quantize_model_kcenter_rare_keep_override
# Environment range: parse-unsigned-long

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
short_output="${artifact_dir}/0074-quantize-center-rare_keep-short-$$.six"
env_output="${artifact_dir}/0074-quantize-center-rare_keep-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -p 16 "-Qcenter:R8" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "center:rare_keep short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1\|*key=rare_keep\|stored=1\|binding=quantize_model_kcenter_rare_keep,quantize_model_kcenter_rare_keep_override\|value=8*}" != "${short_trace}" || {
    echo "not ok" 1 - "rare_keep short value was not stored"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_RARE_KEEP=8" -p 16 "-Qcenter" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "center:rare_keep env conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1\|*key=rare_keep\|stored=1\|binding=quantize_model_kcenter_rare_keep,quantize_model_kcenter_rare_keep_override\|value=8*}" != "${env_trace}" || {
    echo "not ok" 1 - "rare_keep environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_RARE_KEEP=-0" \
    -p 16 "-Qcenter" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "center:rare_keep signed-zero conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1\|*key=rare_keep\|stored=1\|binding=quantize_model_kcenter_rare_keep,quantize_model_kcenter_rare_keep_override\|value=0*}" != "${range_trace}" || {
    echo "not ok" 1 - "center rare_keep did not preserve unsigned-long parsing"
    exit 0
}

width_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_KCENTER_RARE_KEEP=4294967296" \
    -p 16 "-Qcenter" "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "center:rare_keep width conversion failed"
    exit 0
}

test "${width_trace#*LSXSUB1\|*key=rare_keep\|stored=1}" = "${width_trace}" || {
    echo "not ok" 1 - "center rare_keep accepted an over-width value"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "center:rare_keep short and env output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "center:rare_keep image quality regressed"
    printf "# %s\\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "center:rare_keep preserves image output"
exit 0
