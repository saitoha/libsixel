#!/bin/sh
# TAP test for loader fallback when libjpeg 12-bit API is unavailable.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

test "${HAVE_JPEG12_API-}" = 1 && {
    printf "1..0 # SKIP libjpeg 12-bit API is available\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-12bit.jpg"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake-jpeg-12bit-fallback.six"

trace_log=$(set +xv; run_img2sixel -v -L libjpeg,builtin! \
    "${input_jpeg}" -o "${output_sixel}" 2>&1 || true)

case "${trace_log}" in
    *"libsixel: trying libjpeg loader"*)
        ;;
    *)
        echo "not ok" 1 - "libjpeg loader was not attempted first"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: loader libjpeg failed ("*)
        ;;
    *)
        echo "not ok" 1 - "libjpeg failure was not reported"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: trying builtin loader"*)
        ;;
    *)
        echo "not ok" 1 - "fallback to builtin loader did not occur"
        exit 0
        ;;
esac

echo "ok" 1 - "12-bit input falls back after libjpeg failure when API is unavailable"
exit 0
