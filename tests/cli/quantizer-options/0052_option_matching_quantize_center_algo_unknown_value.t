#!/bin/sh
# TAP test verifying unknown center algo values are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qcenter:Ainvalid \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) && {
    printf '\n%s\n' "not ok 1 - unknown center algo value unexpectedly succeeded"
    exit 0
}

case "${msg}" in *unknown\ suboption\ value**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing unknown center algo diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in *\"algo\"**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing center algo key diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

case "${msg}" in *valid\ values*auto,\ fft,\ swap,\ hybrid**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing center algo candidate list"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

printf '\n%s\n' "ok 1 - unknown center algo value is rejected"
exit 0
