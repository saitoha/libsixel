#!/bin/sh
# TAP test verifying -Q accepts medoids base value.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qmedoids \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >/dev/null || {
    echo "not ok" 1 - "-Q medoids base value was rejected"
    exit 0
}

echo "ok" 1 - "-Q accepts medoids base value"
exit 0
