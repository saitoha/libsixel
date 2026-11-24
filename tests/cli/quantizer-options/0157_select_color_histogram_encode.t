#!/bin/sh
# Encode a small image with the documented replacement spelling.
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel unavailable"
    exit 0
}
echo "1..1"
set -v
wire=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -7 -s histogram \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm") || {
    printf '\n%s\n' "not ok 1 - -s histogram failed to encode the image"
    exit 0
}
# A successful conversion must emit a SIXEL DCS and its terminator.
esc=$(printf '\033')
case "$wire" in
    *"${esc}P"*q*"${esc}\\"*) ;;
    *) printf '\n%s\n' "not ok 1 - missing SIXEL output for -s histogram"; exit 0 ;;
esac
printf '\n%s\n' "ok 1 - -s histogram encodes the image"
exit 0
