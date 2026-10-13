#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "ok 1 # SKIP img2sixel disabled"
    exit 0
}
status=0
result=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Ua </dev/null 2>&1) || status=$?
test "${status}" != 0 || {
    echo "not ok 1 - ambiguous output name succeeded"
    exit 0
}
case "${result}" in
    *"a matches auto and adobe-rgb"*) ;;
    *) echo "not ok 1 - missing ambiguity diagnostic"; exit 0 ;;
esac
echo "ok 1 - ambiguous output prefix a rejected"
exit 0
