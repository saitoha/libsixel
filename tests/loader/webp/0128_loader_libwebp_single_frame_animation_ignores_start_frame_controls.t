#!/bin/sh
# TAP test: single-frame animation ignores start-frame CLI/env controls.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-1frame-anim-min.webp"
out_default="${ARTIFACT_LOCAL_DIR}/webp-anim1-default.six"
out_env_valid="${ARTIFACT_LOCAL_DIR}/webp-anim1-env-valid.six"
out_env_invalid="${ARTIFACT_LOCAL_DIR}/webp-anim1-env-invalid.six"
out_env_oob="${ARTIFACT_LOCAL_DIR}/webp-anim1-env-oob.six"
out_cli_valid="${ARTIFACT_LOCAL_DIR}/webp-anim1-cli-valid.six"
out_cli_oob="${ARTIFACT_LOCAL_DIR}/webp-anim1-cli-oob.six"
out_cli_env_invalid="${ARTIFACT_LOCAL_DIR}/webp-anim1-cli-env-invalid.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" >"${out_default}" || {
    echo "not ok" 1 - "baseline single-frame animation decode failed"
    exit 0
}

SIXEL_LOADER_ANIMATION_START_FRAME_NO=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" >"${out_env_valid}" || {
    echo "not ok" 1 - "single-frame animation decode failed with valid start-frame env"
    exit 0
}

SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" >"${out_env_invalid}" || {
    echo "not ok" 1 - "single-frame animation decode failed with invalid start-frame env"
    exit 0
}

SIXEL_LOADER_ANIMATION_START_FRAME_NO=999999999999999999999999999999 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" >"${out_env_oob}" || {
    echo "not ok" 1 - "single-frame animation decode failed with out-of-range start-frame env"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=0 -L libwebp! -ldisable "${image_webp}" >"${out_cli_valid}" || {
    echo "not ok" 1 - "single-frame animation decode failed with --start-frame=0"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=999 -L libwebp! -ldisable "${image_webp}" >"${out_cli_oob}" || {
    echo "not ok" 1 - "single-frame animation decode failed with out-of-range --start-frame"
    exit 0
}

SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=999 -L libwebp! -ldisable "${image_webp}" >"${out_cli_env_invalid}" || {
    echo "not ok" 1 - "single-frame animation decode failed with invalid env and out-of-range --start-frame"
    exit 0
}

cmp -s "${out_default}" "${out_env_valid}" || {
    echo "not ok" 1 - "valid start-frame env unexpectedly changed single-frame animation output"
    exit 0
}

cmp -s "${out_default}" "${out_env_invalid}" || {
    echo "not ok" 1 - "invalid start-frame env unexpectedly changed single-frame animation output"
    exit 0
}

cmp -s "${out_default}" "${out_env_oob}" || {
    echo "not ok" 1 - "out-of-range start-frame env unexpectedly changed single-frame animation output"
    exit 0
}

cmp -s "${out_default}" "${out_cli_valid}" || {
    echo "not ok" 1 - "--start-frame=0 unexpectedly changed single-frame animation output"
    exit 0
}

cmp -s "${out_default}" "${out_cli_oob}" || {
    echo "not ok" 1 - "out-of-range --start-frame unexpectedly changed single-frame animation output"
    exit 0
}

cmp -s "${out_default}" "${out_cli_env_invalid}" || {
    echo "not ok" 1 - "combined invalid env/out-of-range --start-frame unexpectedly changed single-frame animation output"
    exit 0
}

echo "ok" 1 - "single-frame animation ignores start-frame CLI/env controls"
exit 0
