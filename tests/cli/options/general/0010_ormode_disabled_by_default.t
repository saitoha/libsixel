#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Verify default output does not select the OR-mode DCS header.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -o - \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - OR encode failed"
    exit 0
}

case "${output}" in
    *"P7;5q"*) echo "not ok 1 - default selected OR mode"; exit 0 ;;
esac

test -n "${output}" || {
    echo "not ok 1 - default output is empty"
    exit 0
}

echo "ok 1 - default leaves OR mode disabled"
exit 0
