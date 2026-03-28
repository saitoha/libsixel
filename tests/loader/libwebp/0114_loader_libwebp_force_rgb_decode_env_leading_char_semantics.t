#!/bin/sh
# TAP test: force-rgb env evaluates only the first leading character.

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

msg_prefixed_true=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=' true' \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy decode failed with prefixed force-rgb env"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_prefixed_true}" >&2
    exit 0
}

case "${msg_prefixed_true}" in
    *"static decode path=lossy_yuv "*)
        ;;
    *)
        echo "not ok" 1 - "prefixed force-rgb env unexpectedly switched decode path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_prefixed_true}" >&2
        exit 0
        ;;
esac

msg_truthy_suffix=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1foo \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy decode failed with truthy suffix force-rgb env"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_truthy_suffix}" >&2
    exit 0
}

case "${msg_truthy_suffix}" in
    *"static decode path=rgb_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "truthy leading char with suffix did not switch decode path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_truthy_suffix}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "force-rgb env leading-char semantics are applied as expected"
exit 0
