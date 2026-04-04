#!/bin/sh
# TAP test verifying -Q accepts the medoids prefix shorthand.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qm \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    >/dev/null || {
    echo "not ok" 1 - "-Q medoids prefix value was rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts medoids prefix value"
exit 0
