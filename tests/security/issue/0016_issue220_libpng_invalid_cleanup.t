#!/bin/sh
# TAP test for issue #220 item4 (SEGV when using libpng).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
printf '1..1\n'
set -v

issue220="${TOP_SRCDIR}/tests/data/security/issue/data/220/poc4_libpng_invalid_header.png"

set +e
run_img2sixel -Llibpng! "${issue220}" -o /dev/null
command_status=$?
set -e

test "${command_status}" -ge 1 -a "${command_status}" -le 3 || {
    echo "not ok" 1 - "issue #220 item4 did not return mapped error status"
    exit 0
}

echo "ok" 1 - "issue #220 item4 rejected with mapped error status"

exit 0
