#!/bin/sh
# TAP test: force-rgb env switches static lossy decode-path selection.

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
image_lossy_alpha="${TOP_SRCDIR}/tests/data/inputs/formats/webp-static-alpha-keycolor-lossy.webp"

msg_rgb_default=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy RGB static decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_rgb_default}" >&2
    exit 0
}

case "${msg_rgb_default}" in
    *"static decode path=lossy_yuv "*)
        ;;
    *)
        echo "not ok" 1 - "expected lossy_yuv trace was missing for default non-alpha decode"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_rgb_default}" >&2
        exit 0
        ;;
esac

msg_rgb_forced=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "forced-rgb non-alpha static decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_rgb_forced}" >&2
    exit 0
}

case "${msg_rgb_forced}" in
    *"static decode path=rgb_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "expected rgb_u8 trace was missing for forced-rgb non-alpha decode"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_rgb_forced}" >&2
        exit 0
        ;;
esac

case "${msg_rgb_forced}" in
    *"static decode path=lossy_yuv "*)
        echo "not ok" 1 - "forced-rgb non-alpha decode unexpectedly stayed on lossy_yuv path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_rgb_forced}" >&2
        exit 0
        ;;
    *)
        ;;
esac

msg_alpha_bg_default=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S -B#000 "${image_lossy_alpha}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy alpha static decode with -B failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_alpha_bg_default}" >&2
    exit 0
}

case "${msg_alpha_bg_default}" in
    *"static decode path=lossy_yuva "*)
        ;;
    *)
        echo "not ok" 1 - "expected lossy_yuva trace was missing for default alpha/bg decode"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_alpha_bg_default}" >&2
        exit 0
        ;;
esac

msg_alpha_bg_forced=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S -B#000 "${image_lossy_alpha}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "forced-rgb alpha/bg static decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_alpha_bg_forced}" >&2
    exit 0
}

case "${msg_alpha_bg_forced}" in
    *"static decode path=rgba_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "expected rgba_u8 trace was missing for forced-rgb alpha/bg decode"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_alpha_bg_forced}" >&2
        exit 0
        ;;
esac

case "${msg_alpha_bg_forced}" in
    *"static decode path=lossy_yuva "*)
        echo "not ok" 1 - "forced-rgb alpha/bg decode unexpectedly stayed on lossy_yuva path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_alpha_bg_forced}" >&2
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "force-rgb env switches static lossy decode-path selection"
exit 0
