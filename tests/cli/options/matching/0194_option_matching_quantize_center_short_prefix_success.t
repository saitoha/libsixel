#!/bin/sh
# TAP test verifying -Q accepts center unique prefix c.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qc \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    >/dev/null || {
    echo "not ok" 1 - "-Q c unique prefix was rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts center unique prefix c"
exit 0
