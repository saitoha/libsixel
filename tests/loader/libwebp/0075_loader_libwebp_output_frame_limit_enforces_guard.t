#!/bin/sh
# TAP test: libwebp output-frame limit env enforces the emitted-frame guard.

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

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"

msg=$(set +xv; SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "output-frame limit=1 unexpectedly allowed all frames"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"load_with_libwebp: emitted frame count exceeds safety limit."*)
        echo "ok" 1 - "output-frame limit=1 enforces safety guard"
        ;;
    *)
        echo "not ok" 1 - "expected emitted-frame limit diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

exit 0
