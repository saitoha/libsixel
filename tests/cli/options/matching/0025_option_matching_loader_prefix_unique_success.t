#!/bin/sh
# TAP test verifying unique -L loader prefixes are accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbui! \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null || {
    echo "not ok" 1 - "unique -L loader prefix was rejected"
    exit 0
}

echo "ok" 1 - "unique -L loader prefix is accepted"
exit 0
