#!/bin/sh
# Verify -O compatibility mode conversion succeeds.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -O --outfile=/dev/null <"${snake_jpg}" >/dev/null || {
    echo "not ok" 1 - "-O mode conversion failed"
    exit 0
}

echo "ok" 1 - "-O mode conversion succeeded"
exit 0
