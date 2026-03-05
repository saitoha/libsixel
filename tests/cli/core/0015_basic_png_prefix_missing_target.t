#!/bin/sh
# TAP test ensuring an empty png: prefix is rejected.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_sixel2png -o "png:" <"${TOP_SRCDIR}/images/map8.six" \
        2>&1) && {
    echo "not ok" 1 "accepts empty png: prefix"
    exit 0
}

case "${msg}" in
    *'missing target after the "png:" prefix'*)
        ;;
    *)
        echo "not ok" 1 "missing png prefix diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 "rejects empty png prefix"
exit 0
