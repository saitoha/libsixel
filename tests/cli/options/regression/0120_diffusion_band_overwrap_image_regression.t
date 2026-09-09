#!/bin/sh
# Verify common diffusion band_overwrap through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_DIFFUSION|NULL|band_overwrap
# Registry binding: dither_parallel_band_overwrap|dither_parallel_band_overwrap_override
# Environment range: clamp-maximum
# Dither contract: diffuse=fs|*band_overlap=4|band_overwrap=4|band_overwrap_override=1

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
short_output="${artifact_dir}/0120-diffusion-band-overwrap-short-$$.six"
env_output="${artifact_dir}/0120-diffusion-band-overwrap-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    -d "fs:O4" "${input_image}" 2>&1 >"${short_output}") || {
    echo "not ok" 1 - "band_overwrap short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1\|*key=band_overwrap\|stored=1\|binding=dither_parallel_band_overwrap,dither_parallel_band_overwrap_override\|value=4*}" != "${short_trace}" || {
    echo "not ok" 1 - "band_overwrap short value was not stored"
    exit 0
}

test "${short_trace#*LSXDTH1\|*diffuse=fs\|*band_overlap=4\|band_overwrap=4\|band_overwrap_override=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "band_overwrap short value was not effective"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=4" -d "fs" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "band_overwrap environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1\|*key=band_overwrap\|stored=1\|binding=dither_parallel_band_overwrap,dither_parallel_band_overwrap_override\|value=4*}" != "${env_trace}" || {
    echo "not ok" 1 - "band_overwrap environment value was not stored"
    exit 0
}

test "${env_trace#*LSXDTH1\|*diffuse=fs\|*band_overlap=4\|band_overwrap=4\|band_overwrap_override=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "band_overwrap environment value was not effective"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "band_overwrap short and environment output differ"
    exit 0
}

range_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=-1" -d "fs" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "band_overwrap negative conversion failed"
    exit 0
}

test "${range_trace#*LSXSUB1\|*key=band_overwrap\|stored=1*}" = "${range_trace}" || {
    echo "not ok" 1 - "band_overwrap accepted a negative environment value"
    exit 0
}

test "${range_trace#*LSXDTH1\|*band_overlap=0\|band_overwrap=0\|band_overwrap_override=0*}" != "${range_trace}" || {
    echo "not ok" 1 - "invalid band_overwrap changed the default"
    exit 0
}

zero_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env "SIXEL_THREADS=6" \
    --env "SIXEL_DITHER_PARALLEL_BAND_OVERWRAP=-0" -d "fs" \
    "${input_image}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "band_overwrap signed-zero conversion failed"
    exit 0
}

test "${zero_trace#*LSXSUB1\|*key=band_overwrap\|stored=1\|binding=dither_parallel_band_overwrap,dither_parallel_band_overwrap_override\|value=0*}" != "${zero_trace}" || {
    echo "not ok" 1 - "band_overwrap did not preserve signed zero"
    exit 0
}

test "${zero_trace#*LSXDTH1\|*band_overlap=0\|band_overwrap=0\|band_overwrap_override=1*}" != "${zero_trace}" || {
    echo "not ok" 1 - "signed-zero band_overwrap was not effective"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "band_overwrap image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "band_overwrap preserves effective image output"
exit 0
