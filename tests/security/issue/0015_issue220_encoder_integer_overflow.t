#!/bin/sh
# TAP test for issue #220 item3 (integer overflow in encoder.c).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v

issue220="${TOP_SRCDIR}/tests/data/security/issue/data/220/poc3_encoder_integer_overflow.gif"

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -h 1890177820 -e -I -C 1 -S -o /dev/null "${issue220}"
command_status=$?
set -e

test "${command_status}" -ge 1 -a "${command_status}" -le 3 || {
    echo "not ok" 1 - "issue #220 item3 did not return mapped error status"
    exit 0
}

echo "ok" 1 - "issue #220 item3 rejected with mapped error status"

exit 0
