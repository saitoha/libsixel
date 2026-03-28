#!/bin/sh
# TAP test: libwebp rejects invalid start-frame environment values.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..2"
set -v

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"

msg=$(set +xv; SIXEL_LOADER_ANIMATION_START_FRAME_NO=abc \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "non-integer start-frame env unexpectedly succeeded"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer."*)
        echo "ok" 1 - "libwebp rejects non-integer start-frame env"
        ;;
    *)
        echo "not ok" 1 - "expected non-integer diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

msg=$(set +xv; SIXEL_LOADER_ANIMATION_START_FRAME_NO=999999999999999999999999999999 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" \
    2>&1 >/dev/null) && {
    echo "not ok" 2 - "out-of-range start-frame env unexpectedly succeeded"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range."*)
        echo "ok" 2 - "libwebp rejects out-of-range start-frame env"
        ;;
    *)
        echo "not ok" 2 - "expected out-of-range diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

exit 0
