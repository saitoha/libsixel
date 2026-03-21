#!/bin/sh
# TAP test verifying unknown -L base tokens include token and candidates.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel -Lzzzloader \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown -L base token unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"unknown option base value"*"\"zzzloader\""*"valid values"*"builtin"*)
        ;;
    *)
        echo "not ok" 1 - "missing token/candidate details for unknown -L base token"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "unknown -L base token reports token and candidates"
exit 0
