#!/bin/sh
# TAP test verifying unknown -L suboption keys are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel -Lbuiltin:z=1 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown -L suboption key unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"unknown suboption key"*)
        ;;
    *)
        echo "not ok" 1 - "missing unknown -L suboption key diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "unknown -L suboption key is rejected"
exit 0
