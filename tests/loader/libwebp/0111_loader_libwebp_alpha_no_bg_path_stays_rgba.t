#!/bin/sh
# TAP test: lossy alpha without background stays on RGBA path with/without force-rgb.

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

msg_default=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_lossy_alpha}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy alpha static decode without force-rgb failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_default}" >&2
    exit 0
}

case "${msg_default}" in
    *"static decode path=rgba_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "default lossy alpha/no-bg decode did not use rgba_u8 path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_default}" >&2
        exit 0
        ;;
esac

msg_forced=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_lossy_alpha}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy alpha static decode with force-rgb failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_forced}" >&2
    exit 0
}

case "${msg_forced}" in
    *"static decode path=rgba_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "forced-rgb lossy alpha/no-bg decode did not use rgba_u8 path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_forced}" >&2
        exit 0
        ;;
esac

case "${msg_forced}" in
    *"static decode path=lossy_yuva "*)
        echo "not ok" 1 - "forced-rgb alpha/no-bg decode unexpectedly used lossy_yuva path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_forced}" >&2
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "lossy alpha/no-bg decode stays on rgba_u8 path with and without force-rgb"
exit 0
