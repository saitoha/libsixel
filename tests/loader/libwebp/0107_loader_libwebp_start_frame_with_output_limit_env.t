#!/bin/sh
# TAP test: --start-frame path is stable with output-frame-limit env values.

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
out_start1="${ARTIFACT_LOCAL_DIR}/webp-start1-output-limit-default.six"
out_start1_limit1="${ARTIFACT_LOCAL_DIR}/webp-start1-output-limit-1.six"
out_start1_limit_invalid="${ARTIFACT_LOCAL_DIR}/webp-start1-output-limit-invalid.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -ldisable \
    "${image_webp}" >"${out_start1}" || {
    echo "not ok" 1 - "baseline libwebp decode with --start-frame=1 failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -ldisable \
    "${image_webp}" >"${out_start1_limit1}" || {
    echo "not ok" 1 - "decode with --start-frame=1 and output-frame-limit=1 failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -ldisable \
    "${image_webp}" >"${out_start1_limit_invalid}" || {
    echo "not ok" 1 - "decode with --start-frame=1 and invalid output-frame-limit failed"
    exit 0
}

cmp -s "${out_start1}" "${out_start1_limit1}" || {
    echo "not ok" 1 - "output-frame-limit=1 changed --start-frame=1 output unexpectedly"
    exit 0
}

cmp -s "${out_start1}" "${out_start1_limit_invalid}" || {
    echo "not ok" 1 - "invalid output-frame-limit changed --start-frame=1 output unexpectedly"
    exit 0
}

echo "ok" 1 - "--start-frame output is stable across output-frame-limit env values"
exit 0
