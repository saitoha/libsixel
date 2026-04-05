#!/bin/sh
# TAP test verifying -F and -Q*:merge follow argument-order last-wins.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

last_q=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Fauto -Qkmeans:merge=ward \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
last_f=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:merge=ward -Fauto \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
q_only=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:merge=ward \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
f_only=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans -Fauto \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)

test "${last_q}" != "${last_f}" || {
    echo "not ok" 1 - "argument order did not affect merge selection"
    exit 0
}

test "${last_q}" = "${q_only}" || {
    echo "not ok" 1 - "late -Q merge did not override early -F"
    exit 0
}

test "${last_f}" = "${f_only}" || {
    echo "not ok" 1 - "late -F did not override early -Q merge"
    exit 0
}

echo "ok" 1 - "-F and -Q merge use argument-order last-wins"
exit 0
