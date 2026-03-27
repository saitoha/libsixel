#!/bin/sh
# TAP test: coregraphics handles disable/update flag combination.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L coregraphics! -ldisable -dnone -u \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" -o/dev/null || {
    echo "not ok" 1 - "coregraphics disable/update flag combination failed"
    exit 0
}

echo "ok" 1 - "coregraphics disable/update flag combination succeeded"
exit 0
