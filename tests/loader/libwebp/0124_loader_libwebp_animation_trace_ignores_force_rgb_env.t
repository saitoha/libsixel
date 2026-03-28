#!/bin/sh
# TAP test: force-rgb env does not route animation decode through static trace paths.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-8x8-2frame-min.webp"

msg_default=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "default libwebp animation decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_default}" >&2
    exit 0
}

case "${msg_default}" in
    *"animation background source=none"*)
        ;;
    *)
        echo "not ok" 1 - "default animation trace missing background source marker"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_default}" >&2
        exit 0
        ;;
esac

case "${msg_default}" in
    *"finalize frame_no=0 "*"finalize frame_no=1 "*)
        ;;
    *)
        echo "not ok" 1 - "default animation trace missing expected frame finalize markers"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_default}" >&2
        exit 0
        ;;
esac

case "${msg_default}" in
    *"static decode path="*)
        echo "not ok" 1 - "default animation trace unexpectedly reported static decode path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_default}" >&2
        exit 0
        ;;
    *)
        ;;
esac

msg_forced=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" 2>&1 >/dev/null) || {
    echo "not ok" 1 - "forced-rgb libwebp animation decode failed"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg_forced}" >&2
    exit 0
}

case "${msg_forced}" in
    *"animation background source=none"*)
        ;;
    *)
        echo "not ok" 1 - "forced-rgb animation trace missing background source marker"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_forced}" >&2
        exit 0
        ;;
esac

case "${msg_forced}" in
    *"finalize frame_no=0 "*"finalize frame_no=1 "*)
        ;;
    *)
        echo "not ok" 1 - "forced-rgb animation trace missing expected frame finalize markers"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_forced}" >&2
        exit 0
        ;;
esac

case "${msg_forced}" in
    *"static decode path="*)
        echo "not ok" 1 - "forced-rgb animation trace unexpectedly reported static decode path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg_forced}" >&2
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "animation decode trace stays on animation path with and without force-rgb env"
exit 0
