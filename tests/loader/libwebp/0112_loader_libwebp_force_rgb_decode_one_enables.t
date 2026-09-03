#!/bin/sh
# TAP test: force-rgb value 1 selects the legacy RGB path.

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

msg_true=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode _SIXEL_TEST_LIBWEBP_FORCE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy decode failed with force-rgb value 1"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_true}" >&2
    exit 0
}

case "${msg_true}" in
    *"static decode path=rgb_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "force-rgb value 1 did not select rgb_u8 path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_true}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "force-rgb value 1 selects the legacy RGB path"
exit 0
