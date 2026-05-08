#!/bin/sh
# TAP test: empty stdin mapfiles are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin \
          -m gpl:- "${snake_png}" \
          -o/dev/null < /dev/null 2>&1) && {
    echo "not ok" 1 - "empty stdin mapfile unexpectedly succeeded"
    exit 0
}

test "${msg#*sixel_prepare_specified_palette: mapfile \"-\" is empty.}" != "${msg}" || {
    echo "not ok" 1 - "missing empty stdin mapfile diagnostic"
    exit 0
}

echo "ok" 1 - "empty stdin mapfile is rejected"

exit 0
