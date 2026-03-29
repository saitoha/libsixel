#!/bin/sh
# TAP test: static start-frame path ignores output-frame-limit env values.

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
out_start1="${ARTIFACT_LOCAL_DIR}/webp-static-start1-output-limit-default.six"
out_start1_limit1="${ARTIFACT_LOCAL_DIR}/webp-static-start1-output-limit-1.six"
out_start1_limit_invalid="${ARTIFACT_LOCAL_DIR}/webp-static-start1-output-limit-invalid.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -S \
    "${image_webp}" >"${out_start1}" || {
    echo "not ok" 1 - "baseline static start-frame decode failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -S \
    "${image_webp}" >"${out_start1_limit1}" || {
    echo "not ok" 1 - "static start-frame decode failed with output-frame-limit env=1"
    exit 0
}

SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -S \
    "${image_webp}" >"${out_start1_limit_invalid}" || {
    echo "not ok" 1 - "static start-frame decode failed with invalid output-frame-limit env"
    exit 0
}

cmp -s "${out_start1}" "${out_start1_limit1}" || {
    echo "not ok" 1 - "output-frame-limit env=1 unexpectedly changed static start-frame output"
    exit 0
}

cmp -s "${out_start1}" "${out_start1_limit_invalid}" || {
    echo "not ok" 1 - "invalid output-frame-limit env unexpectedly changed static start-frame output"
    exit 0
}

echo "ok" 1 - "static start-frame path ignores output-frame-limit env values"
exit 0
