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

msg_one=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "single-frame animation decode failed (value 1)"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_one}" >&2
    exit 0
}

case "${msg_one}" in
    *"static decode path=rgb_u8 "*" force_rgb=1"*)
        ;;
    *)
        echo "not ok" 1 - "single-frame value 1 trace did not use rgb_u8"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_one}" >&2
        exit 0
        ;;
esac

msg_zero=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "single-frame animation decode failed (value 0)"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_zero}" >&2
    exit 0
}

case "${msg_zero}" in
    *"static decode path=lossy_yuv "*" force_rgb=0"*)
        ;;
    *)
        echo "not ok" 1 - "single-frame value 0 trace did not use lossy_yuv"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_zero}" >&2
        exit 0
        ;;
esac

msg_empty=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE='' \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "single-frame animation decode failed (empty value)"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_empty}" >&2
    exit 0
}

case "${msg_empty}" in
    *"static decode path=lossy_yuv "*" force_rgb=0"*)
        ;;
    *)
        echo "not ok" 1 - "single-frame empty trace did not use lossy_yuv"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_empty}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "single-frame animation fallback uses expected static decode-path selection"
exit 0
