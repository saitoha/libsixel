#!/bin/sh
# TAP test: force-rgb values 0 and 1 switch alpha+bg path selection.

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

image_lossy_alpha="${TOP_SRCDIR}/tests/data/inputs/formats/webp-static-alpha-keycolor-lossy.webp"

msg_false=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S -B#000 "${image_lossy_alpha}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "alpha+bg decode failed with force-rgb value 0"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_false}" >&2
    exit 0
}

case "${msg_false}" in
    *"static decode path=lossy_yuva "*)
        ;;
    *)
        echo "not ok" 1 - "force-rgb value 0 changed alpha+bg path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_false}" >&2
        exit 0
        ;;
esac

msg_true=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S -B#000 "${image_lossy_alpha}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "alpha+bg decode failed with force-rgb value 1"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_true}" >&2
    exit 0
}

case "${msg_true}" in
    *"static decode path=rgba_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "force-rgb value 1 did not select rgba_u8"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_true}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "force-rgb values switch alpha+bg static decode path"
exit 0
