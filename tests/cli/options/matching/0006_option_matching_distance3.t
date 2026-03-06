#!/bin/sh
# TAP test verifying distance-3 desampling typo is rejected with diagnostics.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel -r zzzzz "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "distance-3 typo unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *'specified desampling method is not supported.'*)
        ;;
    *)
        echo "not ok" 1 - "missing diagnostic for distance-3 typo"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "distance-3 typo reports diagnostic"
exit 0
