#!/bin/sh
# TAP test: libwebp static lossless+ICC decode ignores force-rgb static env toggle.

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-lossless-icc-default.six"
out_forced="${ARTIFACT_LOCAL_DIR}/webp-static-lossless-icc-force-rgb-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! \
    "${image_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline libwebp static lossless+icc decode failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! \
    "${image_webp}" >"${out_forced}" || {
    echo "not ok" 1 - "libwebp static lossless+icc decode failed with force-rgb env"
    exit 0
}

cmp -s "${out_default}" "${out_forced}" || {
    echo "not ok" 1 - "force-rgb env unexpectedly changed static lossless+icc output"
    exit 0
}

echo "ok" 1 - "libwebp static lossless+icc decode ignores force-rgb static env toggle"
exit 0
