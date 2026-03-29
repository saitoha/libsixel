#!/bin/sh
# TAP test: libwebp animation lossless decode ignores force-rgb static env toggle.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-anim-lossless-default.six"
out_forced="${ARTIFACT_LOCAL_DIR}/webp-anim-lossless-force-rgb-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable \
    "${image_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline libwebp lossless animation decode failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable \
    "${image_webp}" >"${out_forced}" || {
    echo "not ok" 1 - "libwebp lossless animation decode failed with force-rgb env"
    exit 0
}

cmp -s "${out_default}" "${out_forced}" || {
    echo "not ok" 1 - "force-rgb env unexpectedly changed libwebp lossless animation output"
    exit 0
}

echo "ok" 1 - "libwebp animation lossless decode ignores force-rgb static env toggle"
exit 0
