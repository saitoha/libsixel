#!/bin/sh
# TAP test confirming forced libwebp loader enforces emitted-frame safety limit.

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

msg=$(set +xv; SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=3 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -lauto "${image_webp}" \
    2>&1 >/dev/null) && {
    echo "not ok" 1 - "forced libwebp loader accepted emitted-frame overflow case"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"load_with_libwebp: emitted frame count exceeds safety limit."*)
        ;;
    *)
        echo "not ok" 1 - "expected emitted-frame limit diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "forced libwebp loader rejects emitted-frame overflow case"

exit 0
