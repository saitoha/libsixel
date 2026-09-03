#!/bin/sh
# Verify palette sample target through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL|NULL|sample_target
# Registry binding: palette_sample_target|palette_sample_override
# Environment range: parse-positive-size
# Palette contract: target=128

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
input_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0140-sample-target-short-$$.six"
env_output="${artifact_dir}/0140-sample-target-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 64 "-Qauto:C128" \
    "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "sample target short conversion failed"
    exit 0
}
test "${short_trace#*LSXSUB1|*key=sample_target|stored=1|binding=palette_sample_target,palette_sample_override|value=128*}" != "${short_trace}" || {
    echo "not ok" 1 - "sample target short value was not stored"
    exit 0
}
test "${short_trace#*LSXSMP1|*target=128*}" != "${short_trace}" || {
    echo "not ok" 1 - "sample target missed its consumer"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SAMPLE_TARGET=128" -p 64 -Qauto \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "sample target environment conversion failed"
    exit 0
}
test "${env_trace#*LSXSUB1|*key=sample_target|stored=1|binding=palette_sample_target,palette_sample_override|value=128*}" != "${env_trace}" || {
    echo "not ok" 1 - "sample target environment value was not stored"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SAMPLE_TARGET=0" -p 64 -Qauto \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "sample target zero conversion failed"
    exit 0
}
test "${range_trace#*LSXSUB1|*key=sample_target|stored=1*}" = "${range_trace}" || {
    echo "not ok" 1 - "sample target zero was not rejected"
    exit 0
}

width_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_PALETTE_SAMPLE_TARGET=18446744073709551616" \
    -p 64 -Qauto "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "sample target overflow conversion failed"
    exit 0
}
test "${width_trace#*LSXSUB1|*key=sample_target|stored=1*}" = "${width_trace}" || {
    echo "not ok" 1 - "sample target overflow was not rejected"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "sample target short and environment output differ"
    exit 0
}
lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${input_image}" "${short_output}" 2>&1) || \
    lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "sample target image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "palette sample target preserves image output"
exit 0
