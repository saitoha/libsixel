#!/bin/sh
# TAP test: libwebp static (-S) decode rejects non-integer start-frame env.

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
out_file="${ARTIFACT_LOCAL_DIR}/webp-static-start-frame-env-invalid.six"

msg=$(set +xv; SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_webp}" \
    2>&1 >"${out_file}") && {
    echo "not ok" 1 - "invalid start-frame env was unexpectedly accepted in static mode"
    exit 0
}

case "${msg}" in
    *"SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer."*)
        ;;
    *)
        echo "not ok" 1 - "expected non-integer diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "static libwebp decode rejects non-integer start-frame env"
exit 0
