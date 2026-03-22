#!/bin/sh
# Verify arithmetic JPEG falls through to builtin and still fails without arithmetic decoder.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

test "${HAVE_JPEG_ARITH_DECODER-}" = 1 && {
    printf "1..0 # SKIP libjpeg arithmetic decoder is available\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-8bit-rgb-seq444-arithmetic.jpg"

set +e
trace_log=$(set +xv; run_img2sixel -v -L libjpeg,builtin! \
    "${input_jpeg}" -o /dev/null 2>&1)
run_status=$?
set -e

test "${run_status}" -ne 0 || {
    echo "not ok" 1 - "arithmetic JPEG unexpectedly decoded without arithmetic decoder"
    exit 0
}

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
        echo "not ok" 1 - "libjpeg arithmetic failure was not reported"
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
    *"libsixel: loader builtin failed ("*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader failure was not reported"
        exit 0
        ;;
esac

echo "ok" 1 - "arithmetic JPEG fails after libjpeg-to-builtin fallback without arithmetic decoder"
exit 0
