#!/bin/sh
# TAP test: libwebp start-frame env positive index is applied.

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
out_default="${ARTIFACT_LOCAL_DIR}/webp-start-frame-env-default.six"
out_env_positive="${ARTIFACT_LOCAL_DIR}/webp-start-frame-env-positive.six"
out_cli_positive="${ARTIFACT_LOCAL_DIR}/webp-start-frame-cli-positive.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable \
    "${image_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline libwebp animation decode failed"
    exit 0
}

SIXEL_LOADER_ANIMATION_START_FRAME_NO=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable \
    "${image_webp}" >"${out_env_positive}" || {
    echo "not ok" 1 - "libwebp decode with start-frame env failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -ldisable \
    "${image_webp}" >"${out_cli_positive}" || {
    echo "not ok" 1 - "libwebp decode with --start-frame=1 failed"
    exit 0
}

cmp -s "${out_default}" "${out_env_positive}" && {
    echo "not ok" 1 - "start-frame env did not change libwebp output"
    exit 0
}

cmp -s "${out_env_positive}" "${out_cli_positive}" || {
    echo "not ok" 1 - "start-frame env output mismatched --start-frame=1"
    exit 0
}

echo "ok" 1 - "libwebp start-frame env positive index is applied"
exit 0
