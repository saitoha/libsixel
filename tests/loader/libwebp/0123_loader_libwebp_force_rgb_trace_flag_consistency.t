#!/bin/sh
# TAP test: static libwebp trace reports force_rgb flag consistently with env semantics.

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

msg_default=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "default static lossy decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_default}" >&2
    exit 0
}

case "${msg_default}" in
    *"static decode path=lossy_yuv "*" force_rgb=0"*)
        ;;
    *)
        echo "not ok" 1 - "default static lossy trace did not report force_rgb=0 with lossy_yuv path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_default}" >&2
        exit 0
        ;;
esac

msg_truthy=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "truthy force-rgb static lossy decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_truthy}" >&2
    exit 0
}

case "${msg_truthy}" in
    *"static decode path=rgb_u8 "*" force_rgb=1"*)
        ;;
    *)
        echo "not ok" 1 - "truthy force-rgb trace did not report force_rgb=1 with rgb_u8 path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_truthy}" >&2
        exit 0
        ;;
esac

msg_falsey=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=n \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "falsey force-rgb static lossy decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_falsey}" >&2
    exit 0
}

case "${msg_falsey}" in
    *"static decode path=lossy_yuv "*" force_rgb=0"*)
        ;;
    *)
        echo "not ok" 1 - "falsey force-rgb trace did not report force_rgb=0 with lossy_yuv path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_falsey}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "static libwebp trace reports force_rgb flag consistently with env semantics"
exit 0
