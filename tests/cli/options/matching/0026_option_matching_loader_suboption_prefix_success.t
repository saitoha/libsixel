#!/bin/sh
# TAP test verifying -L suboption key prefixes are accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_e=auto! \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null || {
    echo "not ok" 1 - "-L suboption key prefix was rejected"
    exit 0
}

echo "ok" 1 - "-L suboption key prefix is accepted"
exit 0
