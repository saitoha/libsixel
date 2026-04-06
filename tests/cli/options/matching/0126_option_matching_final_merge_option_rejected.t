#!/bin/sh
# TAP test verifying removed -F option is rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Fward \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "removed -F option unexpectedly succeeded"
    exit 0
}

test "${msg#*invalid option*}" != "${msg}" || \
    test "${msg#*unrecognized option*}" != "${msg}" || {
    echo "not ok" 1 - "missing option rejection diagnostic for -F"
    exit 0
}

test "${msg#*option -- F*}" != "${msg}" || {
    echo "not ok" 1 - "diagnostic did not mention rejected F option"
    exit 0
}

test "${msg#*final-merge*}" = "${msg}" || {
    echo "not ok" 1 - "diagnostic still referenced removed --final-merge"
    exit 0
}

echo "ok" 1 - "removed -F option is rejected"
exit 0
