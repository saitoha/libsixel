#!/bin/sh
# TAP test for issue #220 item2 (DCS parameter signed integer overflow).

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
printf '1..1\n'
set -v

issue220="${TOP_SRCDIR}/tests/data/security/issue/data/220/poc2_sixel_dcs_signed_integer_overflow.sixel"

set +e
run_sixel2png -i "${issue220}" -o /dev/null
command_status=$?
set -e

test "${command_status}" = 255 -o "${command_status}" = 127 || {
    echo "not ok" 1 - "issue #220 item2 did not return mapped overflow status"
    exit 0
}

echo "ok" 1 - "issue #220 item2 rejected with mapped overflow status"

exit 0
