#!/bin/sh
# TAP test for issue #220 item4 (SEGV when using libpng).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v

issue220="${TOP_SRCDIR}/tests/data/security/issue/data/220/crash_w3_1161_AddressSanitizer_1772622371.bmp"

set +e
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibpng! "${issue220}" -o /dev/null
command_status=$?
set -e

test "${command_status}" -ge 1 -a "${command_status}" -le 3 || {
    echo "not ok" 1 - "issue #220 item4 did not return mapped error status"
    exit 0
}

echo "ok" 1 - "issue #220 item4 rejected with mapped error status"

exit 0
