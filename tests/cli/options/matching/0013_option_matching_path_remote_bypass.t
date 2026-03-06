#!/bin/sh
# TAP test verifying remote paths bypass local existence checks.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_sixel2png -i "https://example.invalid/test.six" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "remote input unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *'path "https://example.invalid/test.six" not found.'*)
        echo "not ok" 1 - "remote path was validated as a local filesystem path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "remote path bypassed local filesystem existence checks"
exit 0
