#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Verify --ormode selects the OR-mode DCS header.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --ormode -o - \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - OR encode failed"
    exit 0
}

case "${output}" in
    *"P7;5q"*) ;;
    *) echo "not ok 1 - --ormode did not select OR mode"; exit 0 ;;
esac

echo "ok 1 - --ormode selects OR mode"
exit 0
