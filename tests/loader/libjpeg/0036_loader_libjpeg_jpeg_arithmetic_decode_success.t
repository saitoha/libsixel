#!/bin/sh
# Verify arithmetic JPEG decodes in libjpeg when arithmetic decoder is available.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

test "${HAVE_JPEG_ARITH_DECODER-}" = 1 || {
    printf "1..0 # SKIP libjpeg arithmetic decoder is unavailable\n"
    exit 0
}


echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-8bit-rgb-seq444-arithmetic.jpg"

set +e
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -L libjpeg,builtin! \
    "${input_jpeg}" -o /dev/null 2>&1)
run_status=$?
set -e

test "${run_status}" -eq 0 || {
    echo "not ok" 1 - "arithmetic JPEG decode failed despite arithmetic decoder support"
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
    *"libsixel: loader libjpeg succeeded"*)
        ;;
    *)
        echo "not ok" 1 - "libjpeg loader did not report success"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: trying builtin loader"*)
        echo "not ok" 1 - "builtin fallback unexpectedly occurred"
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "arithmetic JPEG decodes in libjpeg without fallback"
exit 0
