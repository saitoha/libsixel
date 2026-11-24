#!/bin/sh
# TAP test verifying -d sierra accepts variant=3.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d sierra:variant=3 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o /dev/null || {
    printf '\n%s\n' "not ok 1 - sierra variant=3 was rejected"
    exit 0
}

printf '\n%s\n' "ok 1 - sierra variant=3 is accepted"
exit 0
