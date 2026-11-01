#!/bin/sh
# TAP test: libwebp static decode ignores animation start-frame env values.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-static-default.six"
out_with_env="${ARTIFACT_LOCAL_DIR}/webp-static-with-start-frame-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline static libwebp decode failed"
    exit 0
}

SIXEL_LOADER_ANIMATION_START_FRAME_NO=999 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_webp}" >"${out_with_env}" || {
    echo "not ok" 1 - "static decode failed with animation start-frame env"
    exit 0
}

cmp -s "${out_default}" "${out_with_env}" || {
    echo "not ok" 1 - "animation start-frame env unexpectedly changed static output"
    exit 0
}

echo "ok" 1 - "static libwebp decode ignores animation start-frame env"
exit 0
