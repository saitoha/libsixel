#!/bin/sh
# Check that the retired v1.8.7 spelling gets an option error.
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel unavailable"
    exit 0
}
echo "1..1"
set -v
status=0
message=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -s histgram \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" 2>&1 >/dev/null) || status=$?
test "$status" -eq 2 || {
    printf '\n%s\n' "not ok 1 - expected an option error for -s histgram"
    exit 0
}
case "$message" in
    *histgram*histogram*) ;;
    *) printf '\n%s\n' "not ok 1 - missing spelling and replacement in diagnostic"; exit 0 ;;
esac
printf '\n%s\n' "ok 1 - retired -s histgram identifies the accepted spelling"
exit 0
