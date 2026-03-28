#!/bin/sh
# TAP test: static libwebp trace reports expected lossy path selection.

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

msg_rgb=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! "${image_lossy_rgb}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy RGB static decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_rgb}" >&2
    exit 0
}

case "${msg_rgb}" in
    *"static decode path=lossy_yuv "*)
        ;;
    *)
        echo "not ok" 1 - "expected lossy_yuv trace was missing for non-alpha lossy input"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_rgb}" >&2
        exit 0
        ;;
esac

msg_alpha_no_bg=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_lossy_alpha}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy alpha static decode without -B failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_alpha_no_bg}" >&2
    exit 0
}

case "${msg_alpha_no_bg}" in
    *"static decode path=rgba_u8 "*)
        ;;
    *)
        echo "not ok" 1 - "expected rgba_u8 trace was missing for alpha/no-bg input"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_alpha_no_bg}" >&2
        exit 0
        ;;
esac

msg_alpha_bg=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S -B#000 "${image_lossy_alpha}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "lossy alpha static decode with -B failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_alpha_bg}" >&2
    exit 0
}

case "${msg_alpha_bg}" in
    *"static decode path=lossy_yuva "*)
        ;;
    *)
        echo "not ok" 1 - "expected lossy_yuva trace was missing for alpha/bg input"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_alpha_bg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "static libwebp trace reports lossy path selection correctly"
exit 0
