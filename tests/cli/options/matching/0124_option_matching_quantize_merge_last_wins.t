#!/bin/sh
# TAP test verifying repeated -Q merge suboptions follow argument-order
# last-wins.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

last_ward=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=1:restarts=1:feedback=off:merge=auto \
    -Qkmeans:seed=1:restarts=1:feedback=off:merge=ward \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
last_auto=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=1:restarts=1:feedback=off:merge=ward \
    -Qkmeans:seed=1:restarts=1:feedback=off:merge=auto \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
ward_only=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=1:restarts=1:feedback=off:merge=ward \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
auto_only=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=1:restarts=1:feedback=off:merge=auto \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)

test "${last_ward}" != "${last_auto}" || {
    echo "not ok" 1 - "argument order did not affect merge selection"
    exit 0
}

test "${last_ward}" = "${ward_only}" || {
    echo "not ok" 1 - "late ward merge did not override early auto merge"
    exit 0
}

test "${last_auto}" = "${auto_only}" || {
    echo "not ok" 1 - "late auto merge did not override early ward merge"
    exit 0
}

echo "ok" 1 - "-Q merge suboptions use argument-order last-wins"
exit 0
