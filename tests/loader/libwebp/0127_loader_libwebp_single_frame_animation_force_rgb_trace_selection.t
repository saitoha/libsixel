#!/bin/sh
# TAP test: single-frame animation falls back to static decode-path selection.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-1frame-anim-min.webp"

msg_default=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "single-frame animation decode failed (default)"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_default}" >&2
    exit 0
}

case "${msg_default}" in
    *"static decode path=lossy_yuv "*" force_rgb=0"*)
        ;;
    *)
        echo "not ok" 1 - "single-frame animation default trace did not use static lossy_yuv force_rgb=0"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_default}" >&2
        exit 0
        ;;
esac

msg_truthy=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "single-frame animation decode failed (truthy force-rgb)"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_truthy}" >&2
    exit 0
}

case "${msg_truthy}" in
    *"static decode path=rgb_u8 "*" force_rgb=1"*)
        ;;
    *)
        echo "not ok" 1 - "single-frame animation truthy trace did not use static rgb_u8 force_rgb=1"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_truthy}" >&2
        exit 0
        ;;
esac

msg_falsey=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=n \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "single-frame animation decode failed (falsey force-rgb)"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_falsey}" >&2
    exit 0
}

case "${msg_falsey}" in
    *"static decode path=lossy_yuv "*" force_rgb=0"*)
        ;;
    *)
        echo "not ok" 1 - "single-frame animation falsey trace did not use static lossy_yuv force_rgb=0"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_falsey}" >&2
        exit 0
        ;;
esac

msg_prefixed=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=' true' \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "single-frame animation decode failed (prefixed force-rgb)"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_prefixed}" >&2
    exit 0
}

case "${msg_prefixed}" in
    *"static decode path=lossy_yuv "*" force_rgb=0"*)
        ;;
    *)
        echo "not ok" 1 - "single-frame animation prefixed trace did not use static lossy_yuv force_rgb=0"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_prefixed}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "single-frame animation fallback uses expected static decode-path selection"
exit 0
