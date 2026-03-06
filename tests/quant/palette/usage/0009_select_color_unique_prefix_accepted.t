#!/bin/sh
# Confirm unique select-color prefix is accepted.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"

run_img2sixel -shist "${snake_jpg}" >/dev/null || {
    echo "not ok" 1 - "unique select-color prefix failed"
    exit 0
}

echo "ok" 1 - "unique select-color prefix accepted"

exit 0
