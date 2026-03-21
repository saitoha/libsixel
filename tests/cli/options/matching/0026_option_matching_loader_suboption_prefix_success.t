#!/bin/sh
# TAP test verifying -L suboption key prefixes are accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -Lbuiltin:ena=1! \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null || {
    echo "not ok" 1 - "-L suboption key prefix was rejected"
    exit 0
}

echo "ok" 1 - "-L suboption key prefix is accepted"
exit 0
