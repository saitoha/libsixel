#!/bin/sh
# TAP test for issue #220 item5 (integer overflow in encode.c path).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v

issue220="${TOP_SRCDIR}/tests/data/security/issue/data/220/poc5_encode_integer_overflow.gif"

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -S -o /dev/null "${issue220}"
command_status=$?
set -e

test "${command_status}" -ge 0 -a "${command_status}" -le 3 || {
    echo "not ok" 1 - "issue #220 item5 did not return mapped status"
    exit 0
}

echo "ok" 1 - "issue #220 item5 handled without crash"

exit 0
