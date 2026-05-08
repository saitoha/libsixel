#!/bin/sh
# TAP test ensuring mapfile and built-in palette options conflict.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_palette="${TOP_SRCDIR}/images/map8.six"

set +e
msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          -m "${mapfile_palette}" -bxterm16 -o/dev/null "${input_image}" \
          2>&1 >/dev/null)
command_status=$?
set -e

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "unexpected success: mapfile conflicts with built-in palette"
    exit 0
}

case "${msg}" in
*"option -b, --builtin-palette conflicts with -m, --mapfile."*) ;;
*)
    echo "not ok" 1 - "missing mapfile and built-in palette conflict diagnostic"
    exit 0
    ;;
esac

echo "ok" 1 - "invalid option combination rejected"
exit 0
