#!/bin/sh
# TAP test: libwebp static (-S) decode keeps CLI --start-frame over invalid env.

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
out_cli="${ARTIFACT_LOCAL_DIR}/webp-static-start-frame-cli.six"
out_cli_with_bad_env="${ARTIFACT_LOCAL_DIR}/webp-static-start-frame-cli-with-bad-env.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -S "${image_webp}" >"${out_cli}" || {
    echo "not ok" 1 - "static libwebp decode with --start-frame=1 failed"
    exit 0
}

msg=$(set +xv; SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=1 -L libwebp! -S "${image_webp}" \
    2>&1 >"${out_cli_with_bad_env}") || {
    echo "not ok" 1 - "invalid env unexpectedly overrode static --start-frame"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer."*)
        echo "not ok" 1 - "invalid env should be ignored when static --start-frame is set"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
    *)
        ;;
esac

cmp -s "${out_cli}" "${out_cli_with_bad_env}" || {
    echo "not ok" 1 - "invalid env changed static output despite --start-frame override"
    exit 0
}

echo "ok" 1 - "static libwebp --start-frame overrides invalid env input"
exit 0
