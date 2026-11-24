#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP img2sixel is disabled in this build'
    exit 0
}

echo '1..1'
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qheckbert -d none     --threading-policy=greedy:global_budget=1 -4auto:solid=maybe     "${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp" >/dev/null 2>&1 && {
    printf '\n%s\n' 'not ok 1 - invalid solid value was accepted'
    exit 0
}
printf '\n%s\n' 'ok 1 - invalid solid value is rejected'
exit 0
