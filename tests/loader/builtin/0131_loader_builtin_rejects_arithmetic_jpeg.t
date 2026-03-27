#!/bin/sh
# Verify builtin loader rejects arithmetic-coded JPEG streams.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-8bit-rgb-seq444-arithmetic.jpg"

set +e
trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -L builtin! \
    "${input_jpeg}" -o /dev/null 2>&1)
run_status=$?
set -e

test "${run_status}" -ne 0 || {
    echo "not ok" 1 - "builtin loader unexpectedly decoded arithmetic JPEG"
    exit 0
}

case "${trace_log}" in
    *"libsixel: trying builtin loader"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader was not attempted"
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

echo "ok" 1 - "builtin loader rejects arithmetic-coded JPEG"
exit 0
