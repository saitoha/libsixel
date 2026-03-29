#!/bin/sh
# TAP test: libwebp static (-S) decode rejects out-of-range start-frame env.

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
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"
out_file="${ARTIFACT_LOCAL_DIR}/webp-static-start-frame-env-oob.six"

msg=$(set +xv; SIXEL_LOADER_ANIMATION_START_FRAME_NO=999999999999999999999999999999 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libwebp! -S "${image_webp}" \
    2>&1 >"${out_file}") && {
    echo "not ok" 1 - "out-of-range static start-frame env was unexpectedly accepted"
    exit 0
}

case "${msg}" in
    *"SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range."*)
        ;;
    *)
        echo "not ok" 1 - "expected out-of-range diagnostic was missing"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "static libwebp decode rejects out-of-range start-frame env"
exit 0
