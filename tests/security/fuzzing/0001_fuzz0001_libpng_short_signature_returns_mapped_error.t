#!/bin/sh
# TAP test for fuzz0001: short PNG signature returns mapped error status.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

fuzz_input="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0001/libpng_short_signature.bin"

set +e
run_img2sixel -Llibpng! "${fuzz_input}" -o /dev/null >/dev/null 2>&1
command_status=$?
set -e

if [ "${command_status}" -ge 1 ] && [ "${command_status}" -le 3 ]; then
    echo "ok 1 - fuzz0001 returned mapped error status"
else
    echo "not ok 1 - fuzz0001 did not return mapped error status"
fi

exit 0
