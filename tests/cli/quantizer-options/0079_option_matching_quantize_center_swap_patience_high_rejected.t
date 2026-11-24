#!/bin/sh
# TAP test verifying center swap_patience rejects values above range.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:swap_patience=9 \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    printf '\n%s\n' "not ok 1 - out-of-range center swap_patience unexpectedly succeeded"
    exit 0
}

case "${msg}" in *swap_patience**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing center swap_patience range diagnostic"
    exit 0
}

printf '\n%s\n' "ok 1 - center swap_patience rejects values above range"
exit 0
