#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP requires img2sixel'
    exit 0
}
echo '1..1'
set -v
rc=0
stream=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Gf -Wo \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || rc=$?
test "$rc" -ne 0 || {
    echo 'not ok 1 - incompatible forced GPU colorspace was accepted'
    exit 0
}
test -z "$stream" || {
    echo 'not ok 1 - forced GPU colorspace rejection wrote output'
    exit 0
}
echo 'ok 1 - forced GPU colorspace rejected before DCS'
exit 0
