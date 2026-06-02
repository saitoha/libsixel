#!/bin/sh
set -eux

SIXEL_HDR_CASE_ID='019'
SIXEL_HDR_CASE_LABEL='hdr matrix lsqa case=019 target=gamma ev=2 tonemap=none cms=none fallback=srgb'
SIXEL_HDR_TARGET='gamma'
SIXEL_HDR_EXPOSURE_EV='2'
SIXEL_HDR_TONEMAP='none'
SIXEL_HDR_CMS_ENGINE='none'
SIXEL_HDR_FALLBACK_PROFILE='srgb'

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_hdr="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_gradient16x16.hdr"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin-hdr-lsqa-${SIXEL_HDR_CASE_ID}.six"
black_reference="${ARTIFACT_LOCAL_DIR}/builtin-hdr-black-reference.ppm"
white_reference="${ARTIFACT_LOCAL_DIR}/builtin-hdr-white-reference.ppm"
lsqa_status=0

SIXEL_LOADER_CMS_TARGET_COLORSPACE="${SIXEL_HDR_TARGET}" \
SIXEL_LOADER_PREFER_8BIT=0 \
SIXEL_LOADER_HDR_FALLBACK_PROFILE="${SIXEL_HDR_FALLBACK_PROFILE}" \
SIXEL_LOADER_HDR_EXPOSURE_EV="${SIXEL_HDR_EXPOSURE_EV}" \
SIXEL_LOADER_HDR_TONEMAP="${SIXEL_HDR_TONEMAP}" \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:cms_engine=${SIXEL_HDR_CMS_ENGINE}!" "${input_hdr}" >"${output_sixel}" || {
    echo "not ok 1 - builtin HDR lsqa decode failed (${SIXEL_HDR_CASE_LABEL})"
    exit 0
}

printf 'P3\n1 1\n255\n0 0 0\n' >"${black_reference}"
lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.995" \
    "${black_reference}" "${output_sixel}" 2>&1) || lsqa_status=$?
lsqa_status=${lsqa_status-0}
test "${lsqa_status}" -eq 5 || {
    echo "not ok 1 - builtin HDR lsqa black-collapse risk (${SIXEL_HDR_CASE_LABEL}): ${lsqa_msg}"
    exit 0
}

lsqa_status=0
printf 'P3\n1 1\n255\n255 255 255\n' >"${white_reference}"
lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.995" \
    "${white_reference}" "${output_sixel}" 2>&1) || lsqa_status=$?
lsqa_status=${lsqa_status-0}
test "${lsqa_status}" -eq 5 || {
    echo "not ok 1 - builtin HDR lsqa white-collapse risk (${SIXEL_HDR_CASE_LABEL}): ${lsqa_msg}"
    exit 0
}

echo "ok 1 - builtin HDR lsqa high-risk guard (${SIXEL_HDR_CASE_LABEL})"
exit 0
