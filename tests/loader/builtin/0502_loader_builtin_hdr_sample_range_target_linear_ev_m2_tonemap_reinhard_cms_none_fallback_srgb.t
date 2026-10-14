#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
set -eux

SIXEL_HDR_CASE_ID='010'
SIXEL_HDR_CASE_LABEL='hdr matrix sample range case=010 target=linear ev=-2 tonemap=reinhard cms=none fallback=srgb'
SIXEL_HDR_TARGET='linear'
SIXEL_HDR_EXPOSURE_EV='-2'
SIXEL_HDR_TONEMAP='reinhard'
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
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin-hdr-range-${SIXEL_HDR_CASE_ID}.six"

SIXEL_LOADER_CMS_TARGET_COLORSPACE="${SIXEL_HDR_TARGET}" \
SIXEL_LOADER_PREFER_8BIT=0 \
SIXEL_LOADER_HDR_FALLBACK_PROFILE="${SIXEL_HDR_FALLBACK_PROFILE}" \
SIXEL_LOADER_HDR_EXPOSURE_EV="${SIXEL_HDR_EXPOSURE_EV}" \
SIXEL_LOADER_HDR_TONEMAP="${SIXEL_HDR_TONEMAP}" \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    "-Lbuiltin:cms_engine=${SIXEL_HDR_CMS_ENGINE}!" "${input_hdr}" >"${output_sixel}" || {
    echo "not ok 1 - builtin HDR sample range decode failed (${SIXEL_HDR_CASE_LABEL})"
    exit 0
}

# Observe actual decoded sample variation. An unavailable spatial metric
# cannot be evidence that the image differs from a solid black/white frame.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_sample_range \
    "${output_sixel}" 1 || {
    echo "not ok 1 - HDR output lost its RGB sample range"
    exit 0
}

echo "ok 1 - builtin HDR sample range high-risk guard (${SIXEL_HDR_CASE_LABEL})"
exit 0
