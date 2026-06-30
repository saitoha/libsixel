#!/bin/sh
# Verify transparent-offset rejects transparent policies that clear pixels.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +e

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin -p 2 --transparent-policy=background -+ 3,5 \
    -o - "${input_image}" 2>&1 >/dev/null)
status=$?
set -e

test "${status}" -ne 0 || {
    echo "not ok 1 - transparent-offset accepted background policy"
    exit 0
}

case "${output}" in
    *"transparent-offset requires transparent-policy=keep"*) ;;
    *)
        echo "not ok 1 - transparent-offset conflict message mismatch"
        exit 0
        ;;
esac

echo "ok 1 - transparent-offset rejects background transparent policy"
exit 0
