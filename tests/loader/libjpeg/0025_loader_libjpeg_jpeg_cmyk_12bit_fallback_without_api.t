#!/bin/sh
# Verify fallback to builtin loader for 12-bit CMYK JPEG when libjpeg 12-bit API is unavailable.

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

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-12bit-cmyk-seq444.jpg"

trace_log=$(set +xv; run_img2sixel -v -L libjpeg,builtin! \
    "${input_jpeg}" -o /dev/null 2>&1 || true)

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

case "${trace_log}" in
    *"libsixel: loader builtin succeeded"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader did not succeed after fallback"
        exit 0
        ;;
esac

echo "ok" 1 - "12-bit CMYK input falls back to builtin after libjpeg failure when 12-bit API is unavailable"
exit 0
