#!/bin/sh
# Verify transparent-offset places high-color bodies on the offset geometry.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
esc="$(printf '\033')"

output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin -I -+ 3,5 -o - "${input_image}") || {
    echo "not ok 1 - transparent-offset high-color render failed"
    exit 0
}

case "${output}" in
    "${esc}P0;1q\"1;1;9;17"*) ;;
    *)
        echo "not ok 1 - high-color transparent-offset header geometry mismatch"
        exit 0
        ;;
esac

case "${output}" in
    *"???"*) ;;
    *)
        echo "not ok 1 - high-color transparent-offset dropped the left padding"
        exit 0
        ;;
esac

echo "ok 1 - high-color transparent-offset emits the offset geometry"
exit 0
