#!/bin/sh
# TAP test verifying -Q rejects kmeans binbits greater than 8.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qk:N9 "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) && {
    printf '\n%s\n' "not ok 1 - invalid kmeans binbits unexpectedly succeeded"
    exit 0
}

case "${msg}" in *binbits\ must\ be\ in\ range\ 4-8.**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing kmeans binbits range diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

printf '\n%s\n' "ok 1 - kmeans binbits upper bound is rejected"
exit 0
