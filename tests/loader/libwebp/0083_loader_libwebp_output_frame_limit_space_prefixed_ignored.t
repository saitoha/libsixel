#!/bin/sh
# TAP test: libwebp output-frame limit env ignores space-prefixed values.

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

msg=$(set +xv; SIXEL_TRACE_TOPIC=webp_decode SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES=' 1' \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -ldisable "${image_webp}" \
    2>&1 >/dev/null) || {
    echo "not ok" 1 - "space-prefixed output-frame limit unexpectedly failed decode"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in
    *"ignore invalid SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES= 1"*)
        echo "ok" 1 - "space-prefixed output-frame limit is ignored with trace"
        ;;
    *)
        echo "not ok" 1 - "expected space-prefixed ignore trace was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

exit 0
