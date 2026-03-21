#!/bin/sh
# TAP test verifying unique -L loader prefixes are accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -Lbui! \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null || {
    echo "not ok" 1 - "unique -L loader prefix was rejected"
    exit 0
}

echo "ok" 1 - "unique -L loader prefix is accepted"
exit 0
