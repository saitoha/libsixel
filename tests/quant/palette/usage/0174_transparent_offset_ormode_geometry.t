#!/bin/sh
# Verify transparent-offset places OR-mode bodies on the offset geometry.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin -p 2 -O -+ 3,5 -o - "${input_image}") || {
    echo "not ok 1 - transparent-offset OR-mode render failed"
    exit 0
}

case "${output}" in
    *"\"1;1;9;17"*) ;;
    *)
        echo "not ok 1 - OR-mode transparent-offset header geometry mismatch"
        exit 0
        ;;
esac

case "${output}" in
    *"???"*) ;;
    *)
        echo "not ok 1 - OR-mode transparent-offset dropped the left padding"
        exit 0
        ;;
esac

echo "ok 1 - OR-mode transparent-offset emits the offset geometry"
exit 0
