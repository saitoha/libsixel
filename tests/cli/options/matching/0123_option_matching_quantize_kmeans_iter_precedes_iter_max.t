#!/bin/sh
# TAP test verifying kmeans iter takes precedence over iter_max.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

iter_only=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=1:restarts=1:feedback=off:iter=1 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
iter_and_max=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=1:restarts=1:feedback=off:iter=1:iter_max=100 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
iter_more=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=1:restarts=1:feedback=off:iter=8 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)

test "${iter_only}" = "${iter_and_max}" || {
    echo "not ok" 1 - "iter did not take precedence over iter_max"
    exit 0
}

test "${iter_only}" != "${iter_more}" || {
    echo "not ok" 1 - "iter precedence check lacked an iteration contrast"
    exit 0
}

echo "ok" 1 - "kmeans iter takes precedence over iter_max"
exit 0
