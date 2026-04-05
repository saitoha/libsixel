#!/bin/sh
# TAP test verifying kmeans seed fixes output reproducibly.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

seed_123_a=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=123:restarts=2 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
seed_123_b=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=123:restarts=2 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
seed_124=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qkmeans:seed=124:restarts=2 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)

test "${seed_123_a}" = "${seed_123_b}" || {
    echo "not ok" 1 - "same kmeans seed was not reproducible"
    exit 0
}

test "${seed_123_a}" != "${seed_124}" || {
    echo "not ok" 1 - "different kmeans seed did not affect output"
    exit 0
}

echo "ok" 1 - "kmeans seed makes output reproducible"
exit 0
