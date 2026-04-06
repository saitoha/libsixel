#!/bin/sh
# TAP test verifying -F ward and -Q*:merge=ward remain compatible.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

legacy=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Fward \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)
modern=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qauto:merge=ward \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" | cksum)

test "${legacy}" = "${modern}" || {
    echo "not ok" 1 - "-F ward and -Qauto:merge=ward diverged"
    exit 0
}

echo "ok" 1 - "-F ward stays equivalent to -Qauto:merge=ward"
exit 0
