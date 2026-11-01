#!/bin/sh
# TAP test ensuring built-in palette and monochrome options conflict.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

set +e
msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          -bxterm16 -e -o/dev/null "${input_image}" 2>&1 >/dev/null)
command_status=$?
set -e

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "unexpected success: built-in palette conflicts with monochrome"
    exit 0
}

case "${msg}" in
*"option -e, --monochrome conflicts with -b, --builtin-palette."*) ;;
*)
    echo "not ok" 1 - "missing built-in palette and monochrome conflict diagnostic"
    exit 0
    ;;
esac

echo "ok" 1 - "invalid option combination rejected"
exit 0
