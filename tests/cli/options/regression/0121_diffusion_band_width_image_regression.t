#!/bin/sh
# Verify common diffusion band_width through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|NULL|band_width
# Registry binding: dither_parallel_band_width|dither_parallel_band_width_override
# Environment range: clamp-maximum
# Dither contract: diffuse=fs|*band_height=12|*band_width=9|band_width_override=1

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP threading is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
reference_image="${input_image}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0121-diffusion-band-width-short-$$.six"
env_output="${artifact_dir}/0121-diffusion-band-width-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    -d "fs:B9" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "band_width short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=band_width|stored=1|binding=dither_parallel_band_width,dither_parallel_band_width_override|value=9*}" != "${short_trace}" || {
    echo "not ok" 1 - "band_width short value was not stored"
    exit 0
}

test "${short_trace#*LSXDTH1|*diffuse=fs|*band_height=12|*band_width=9|band_width_override=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "band_width short value was not effective"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_WIDTH=9" -d "fs" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "band_width environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=band_width|stored=1|binding=dither_parallel_band_width,dither_parallel_band_width_override|value=9*}" != "${env_trace}" || {
    echo "not ok" 1 - "band_width environment value was not stored"
    exit 0
}

test "${env_trace#*LSXDTH1|*diffuse=fs|*band_height=12|*band_width=9|band_width_override=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "band_width environment value was not effective"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "band_width short and environment output differ"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_WIDTH=-1" -d "fs" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "negative band_width conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1|*key=band_width|stored=1*}" = "${range_trace}" || {
    echo "not ok" 1 - "band_width accepted a negative environment value"
    exit 0
}

test "${range_trace#*LSXDTH1|*band_height=18|*band_width=0|band_width_override=0*}" != "${range_trace}" || {
    echo "not ok" 1 - "negative band_width changed the default"
    exit 0
}

zero_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_WIDTH=0" -d "fs" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "zero band_width conversion failed"
    exit 0
}

test "${zero_trace#*LSXSUB1|*key=band_width|stored=1*}" = \
    "${zero_trace}" || {
    echo "not ok" 1 - "band_width accepted a zero environment value"
    exit 0
}

test "${zero_trace#*LSXDTH1|*band_height=18|*band_width=0|band_width_override=0*}" != "${zero_trace}" || {
    echo "not ok" 1 - "zero band_width changed the default"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "band_width image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "band_width preserves effective image output"
exit 0
