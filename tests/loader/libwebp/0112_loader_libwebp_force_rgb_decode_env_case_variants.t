#!/bin/sh
# TAP test: force-rgb env respects expected case-variant leading tokens.

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

msg_true_upper=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=T \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy decode failed with uppercase truthy force-rgb env"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_true_upper}" >&2
    exit 0
}

case "${msg_true_upper}" in
    *"static decode path=rgb_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "uppercase truthy force-rgb env did not switch to rgb_u8 path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_true_upper}" >&2
        exit 0
        ;;
esac

msg_false_letter=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=n \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy decode failed with falsey letter force-rgb env"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_false_letter}" >&2
    exit 0
}

case "${msg_false_letter}" in
    *"static decode path=lossy_yuv "*)
        ;;
    *)
        echo "not ok" 1 - "falsey letter force-rgb env unexpectedly switched path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_false_letter}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "force-rgb env case-variant token semantics are applied as expected"
exit 0
