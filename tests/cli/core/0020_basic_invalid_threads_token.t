#!/bin/sh
# TAP test ensuring sixel2png rejects invalid thread tokens.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_sixel2png -= bogus -o/dev/null <"${TOP_SRCDIR}/images/map64.six" \
        2>&1) && {
    echo "not ok" 1 - "accepts invalid thread token"
    exit 0
}

case "${msg}" in
    *"threads must be a positive integer or 'auto'"*)
        ;;
    *)
        echo "not ok" 1 - "missing invalid thread diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "rejects invalid thread token"
exit 0
