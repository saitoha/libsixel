#!/bin/sh
# Verify linear background interpretation changes libwebp alpha composition.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-alpha-8x8-2frame-min.webp"
output_gamma="${ARTIFACT_LOCAL_DIR}/webp-bgcs-alpha-gamma.six"
output_linear="${ARTIFACT_LOCAL_DIR}/webp-bgcs-alpha-linear.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_BACKGROUND_COLORSPACE=gamma \
              -Llibwebp:cms_engine=none! \
              -S \
              -B#808080 "${input_webp}" >"${output_gamma}" || {
    echo "not ok" 1 - "libwebp gamma background composition failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_BACKGROUND_COLORSPACE=linear \
              -Llibwebp:cms_engine=none! \
              -S \
              -B#808080 "${input_webp}" >"${output_linear}" || {
    echo "not ok" 1 - "libwebp linear background composition failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" "${output_gamma}" "${output_linear}" 2>&1) || lsqa_status=$?

test "${lsqa_status-0}" -eq 5 || {
    echo "not ok" 1 - "gamma and linear composition were not distinguishable: ${lsqa_msg-}"
    exit 0
}

echo "ok" 1 - "linear background interpretation changes libwebp composition"
exit 0
