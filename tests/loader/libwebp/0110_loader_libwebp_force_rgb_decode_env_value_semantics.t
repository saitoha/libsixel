#!/bin/sh
# TAP test: force-rgb env honors truthy/falsey leading token semantics.

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

msg_false=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy decode failed with falsey force-rgb env"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_false}" >&2
    exit 0
}

case "${msg_false}" in
    *"static decode path=lossy_yuv "*)
        ;;
    *)
        echo "not ok" 1 - "falsey force-rgb env did not keep lossy_yuv path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_false}" >&2
        exit 0
        ;;
esac

msg_true=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=y \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy decode failed with truthy force-rgb env"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_true}" >&2
    exit 0
}

case "${msg_true}" in
    *"static decode path=rgb_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "truthy force-rgb env did not switch to rgb_u8 path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_true}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "force-rgb env value semantics are applied as expected"
exit 0
