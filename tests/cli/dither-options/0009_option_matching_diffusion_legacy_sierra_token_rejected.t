#!/bin/sh
# TAP test verifying legacy sierraN token is rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -d sierra1 \
        "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
        -o /dev/null 2>&1
) && {
    printf '\n%s\n' "not ok 1 - legacy sierra1 token unexpectedly succeeded"
    exit 0
}

case "${msg}" in *unknown\ option\ base\ value**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - missing legacy sierra token diagnostic"
    exit 0
}

case "${msg}" in *sierra1**) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - legacy sierra token diagnostic lacked rejected value"
    exit 0
}

printf '\n%s\n' "ok 1 - legacy sierra token is rejected"
exit 0
