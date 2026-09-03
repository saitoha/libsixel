#!/bin/sh
# Verify common loader prefer_8bit through short and environment paths.
# Registry row: SIXEL_OPTION_SCHEMA_LOADERS|NULL|prefer_8bit
# Registry binding: cms_prefer_8bit

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}
test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_embedded_srgb_icc_vp8_static_12x8.webp"
reference_image="${input_image}"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
short_output="${artifact_dir}/0110-loader-prefer-8bit-short-$$.six"
env_output="${artifact_dir}/0110-loader-prefer-8bit-env-$$.six"

short_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_PREFER_8BIT=invalid" \
    "-Llibwebp:Tgamma:V1:Eauto!" "${input_image}" \
    2>&1 >"${short_output}") || {
    echo "not ok" 1 - "prefer_8bit short conversion failed"
    exit 0
}

test "${short_trace#*LSXSUB1|*key=prefer_8bit|stored=1|binding=cms_prefer_8bit|value=1*}" != "${short_trace}" || {
    echo "not ok" 1 - "prefer_8bit short value was not stored"
    exit 0
}

test "${short_trace#*LSXLDR1|cms_target=gamma|prefer_8bit=1|pixelformat=rgb888*}" != "${short_trace}" || {
    echo "not ok" 1 - "prefer_8bit short value did not reach the loader"
    exit 0
}

env_trace=$(set +xv; SIXEL_TRACE_TOPIC=suboption_contract,loader_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_CMS_TARGET_COLORSPACE=gamma" \
    --env "SIXEL_LOADER_PREFER_8BIT=1" "-Llibwebp:Eauto!" \
    "${input_image}" 2>&1 >"${env_output}") || {
    echo "not ok" 1 - "prefer_8bit environment conversion failed"
    exit 0
}

test "${env_trace#*LSXSUB1|*key=prefer_8bit|stored=1|binding=cms_prefer_8bit|value=1*}" != "${env_trace}" || {
    echo "not ok" 1 - "prefer_8bit environment value was not stored"
    exit 0
}

test "${env_trace#*LSXLDR1|cms_target=gamma|prefer_8bit=1|pixelformat=rgb888*}" != "${env_trace}" || {
    echo "not ok" 1 - "prefer_8bit environment value did not reach the loader"
    exit 0
}

cmp -s "${short_output}" "${env_output}" || {
    echo "not ok" 1 - "prefer_8bit short and environment output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${short_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "prefer_8bit image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "prefer_8bit preserves image output"
exit 0
