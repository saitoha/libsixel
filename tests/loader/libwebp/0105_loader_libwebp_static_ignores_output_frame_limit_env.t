#!/bin/sh
# TAP test: libwebp static (-S) decode ignores output-frame-limit env values.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-output-limit-default.six"
out_limit1="${ARTIFACT_LOCAL_DIR}/webp-static-output-limit-1.six"
out_limit_invalid="${ARTIFACT_LOCAL_DIR}/webp-static-output-limit-invalid.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline static libwebp decode failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_webp}" >"${out_limit1}" || {
    echo "not ok" 1 - "static decode failed with output-frame-limit env=1"
    exit 0
}

SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_webp}" >"${out_limit_invalid}" || {
    echo "not ok" 1 - "static decode failed with invalid output-frame-limit env"
    exit 0
}

cmp -s "${out_default}" "${out_limit1}" || {
    echo "not ok" 1 - "output-frame-limit env=1 unexpectedly changed static output"
    exit 0
}

cmp -s "${out_default}" "${out_limit_invalid}" || {
    echo "not ok" 1 - "invalid output-frame-limit env unexpectedly changed static output"
    exit 0
}

echo "ok" 1 - "static libwebp decode ignores output-frame-limit env values"
exit 0
