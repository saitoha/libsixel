#!/bin/sh
# TAP test: libwebp CLI start-frame overrides conflicting valid env values.

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
out_env_zero="${ARTIFACT_LOCAL_DIR}/webp-start-frame-env-zero.six"
out_cli_one="${ARTIFACT_LOCAL_DIR}/webp-start-frame-cli-one.six"
out_cli_one_with_env_zero="${ARTIFACT_LOCAL_DIR}/webp-start-frame-cli-one-env-zero.six"

SIXEL_LOADER_ANIMATION_START_FRAME_NO=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable \
    "${image_webp}" >"${out_env_zero}" || {
    echo "not ok" 1 - "libwebp decode with start-frame env=0 failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -ldisable \
    "${image_webp}" >"${out_cli_one}" || {
    echo "not ok" 1 - "libwebp decode with --start-frame=1 failed"
    exit 0
}

SIXEL_LOADER_ANIMATION_START_FRAME_NO=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -ldisable \
    "${image_webp}" >"${out_cli_one_with_env_zero}" || {
    echo "not ok" 1 - "valid env unexpectedly overrode --start-frame=1"
    exit 0
}

cmp -s "${out_env_zero}" "${out_cli_one}" && {
    echo "not ok" 1 - "test setup did not produce conflicting start-frame outputs"
    exit 0
}

cmp -s "${out_cli_one}" "${out_cli_one_with_env_zero}" || {
    echo "not ok" 1 - "valid env changed output despite --start-frame override"
    exit 0
}

echo "ok" 1 - "libwebp --start-frame overrides conflicting valid env input"
exit 0
