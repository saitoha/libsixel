#!/bin/sh
# TAP test: libwebp static (-S) decode applies positive --start-frame.

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
out_static_default="${ARTIFACT_LOCAL_DIR}/webp-static-start-frame-default.six"
out_static_start1="${ARTIFACT_LOCAL_DIR}/webp-static-start-frame-1.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_webp}" >"${out_static_default}" || {
    echo "not ok" 1 - "baseline static libwebp decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -S "${image_webp}" >"${out_static_start1}" || {
    echo "not ok" 1 - "static libwebp decode with --start-frame=1 failed"
    exit 0
}

cmp -s "${out_static_default}" "${out_static_start1}" && {
    echo "not ok" 1 - "static --start-frame=1 did not change output"
    exit 0
}

echo "ok" 1 - "static libwebp decode applies positive --start-frame"
exit 0
