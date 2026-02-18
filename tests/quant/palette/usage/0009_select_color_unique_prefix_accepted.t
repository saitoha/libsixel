#!/bin/sh
# Confirm unique select-color prefix is accepted.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"

run_img2sixel -shist "${snake_jpg}" >/dev/null || {
    fail 1 "unique select-color prefix failed"
    exit 0
}

pass 1 "unique select-color prefix accepted"

exit 0
