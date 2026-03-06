#!/bin/sh
# TAP test verifying fuzzy suggestions can be disabled for invalid choices.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel --env SIXEL_OPTION_FUZZY_SUGGESTIONS=0 \
    -r hamnimg "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "distance-2 typo unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *'specified desampling method is not supported.'*)
        ;;
    *)
        echo "not ok" 1 - "invalid choice still reports fuzzy suggestion"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *'Did you mean:'*)
        echo "not ok" 1 - "invalid choice still reports fuzzy suggestion"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "invalid choice omits fuzzy suggestion when disabled"
exit 0
