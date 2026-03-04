#!/bin/sh
# TAP test: builtin loader accepts update and static-frame flags together.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

run_img2sixel -Lbuiltin! -ldisable -dnone -u -g "${image_gif}" >/dev/null || {
    echo "not ok" 1 "combined update and static frame fails"
    exit 0
}

echo "ok" 1 "combined update and static frame works"
exit 0
