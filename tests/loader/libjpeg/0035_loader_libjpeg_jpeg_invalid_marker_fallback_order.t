#!/bin/sh
# Verify runtime decode failure in libjpeg falls through to builtin loader.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/corrupted/invalid_marker.jpg"

set +e
trace_log=$(set +xv; run_img2sixel -v -L libjpeg,builtin! \
    "${input_jpeg}" -o /dev/null 2>&1)
run_status=$?
set -e

test "${run_status}" -ne 0 || {
    echo "not ok" 1 - "invalid-marker JPEG unexpectedly decoded"
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
        echo "not ok" 1 - "libjpeg decode failure was not reported"
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
        echo "not ok" 1 - "builtin loader failure was not reported after fallback"
        exit 0
        ;;
esac

echo "ok" 1 - "invalid-marker JPEG follows libjpeg-to-builtin fallback order"
exit 0
