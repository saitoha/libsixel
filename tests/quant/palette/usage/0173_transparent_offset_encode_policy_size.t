#!/bin/sh
# Verify encode-policy=size allows transparent-offset padding.

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
    -L builtin -E size -p 2 -+ 3,3 -o - "${input_image}") || {
    echo "not ok 1 - encode-policy=size rejected transparent-offset"
    exit 0
}

case "${output}" in
    "${esc}P0;1q\"1;1;9;15"*) ;;
    *)
        echo "not ok 1 - transparent-offset size header geometry mismatch"
        exit 0
        ;;
esac

case "${output}" in
    *"???"*) ;;
    *)
        echo "not ok 1 - transparent-offset size did not keep left padding"
        exit 0
        ;;
esac

echo "ok 1 - encode-policy=size allows transparent-offset padding"
exit 0
