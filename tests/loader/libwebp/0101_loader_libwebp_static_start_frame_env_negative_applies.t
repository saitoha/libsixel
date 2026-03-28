#!/bin/sh
# TAP test: libwebp static (-S) decode applies negative start-frame env value.

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
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-start-frame-env-neg-default.six"
out_env_neg1="${ARTIFACT_LOCAL_DIR}/webp-static-start-frame-env-neg1.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline static libwebp decode failed"
    exit 0
}

SIXEL_LOADER_ANIMATION_START_FRAME_NO=-1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_webp}" >"${out_env_neg1}" || {
    echo "not ok" 1 - "static libwebp decode with negative start-frame env failed"
    exit 0
}

cmp -s "${out_default}" "${out_env_neg1}" && {
    echo "not ok" 1 - "negative start-frame env did not change static output"
    exit 0
}

echo "ok" 1 - "static libwebp decode applies negative start-frame env"
exit 0
