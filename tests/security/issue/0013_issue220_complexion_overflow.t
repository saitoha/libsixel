#!/bin/sh
# TAP test for issue #220 item6 (complexion arithmetic overflow in quant path).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v

fixture="${TOP_SRCDIR}/tests/data/security/issue/data/220/snake_8x8.png"

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p4 -C 2147483647 -o /dev/null "${fixture}"
command_status=$?
set -e

test "${command_status}" -ge 1 -a "${command_status}" -le 3 || {
    echo "not ok" 1 - "issue #220 item6 did not return mapped error status"
    exit 0
}

echo "ok" 1 - "issue #220 item6 rejected with mapped error status"

exit 0
