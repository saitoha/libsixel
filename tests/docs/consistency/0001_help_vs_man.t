#!/bin/sh
# TAP test comparing img2sixel --help with the manpage options list.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v

sum1=$(run_img2sixel -H | awk '/^-[A-Za-z0-9],/ { print $1, $2; } /^-[A-Za-z0-9] / { print $1, $2, $3; }' | tr -d \\r | cksum)

sum2=$(awk '
/^\.B \\-\\?[A-Za-z0-9],/ { gsub(/\\/, ""); print $2, $3; }
/^\.B \\-\\?[A-Za-z0-9] / { gsub(/\\fP|\\fI|\\/, ""); print $2, $3, $4; }
' "${TOP_SRCDIR}/converters/img2sixel.1" | cksum)

test "${sum1}" = "${sum2}" || {
    fail 1 "--help diverges from manpage"
    exit 0
}

pass 1 "--help matches manpage"
exit 0
