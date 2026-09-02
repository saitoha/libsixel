#!/bin/sh
# TAP test: an empty force-rgb value behaves like an unset variable.

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

image_lossy_rgb="${TOP_SRCDIR}/tests/data/inputs/snake_64.webp"

msg_empty=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE='' \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy decode failed with empty force-rgb env"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_empty}" >&2
    exit 0
}

case "${msg_empty}" in
    *"static decode path=lossy_yuv "*)
        ;;
    *)
        echo "not ok" 1 - "empty force-rgb env changed the default path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_empty}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "empty force-rgb value behaves like an unset variable"
exit 0
