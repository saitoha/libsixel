#!/bin/sh
# TAP test for fuzz0005: empty minimized input does not crash img2sixel.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

fuzz_input="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0005/loader_order_empty.bin"

test -f "${fuzz_input}" || {
    echo "not ok" 1 - "fuzz0005 input is missing"
    exit 0
}

test ! -s "${fuzz_input}" || {
    echo "not ok" 1 - "fuzz0005 input is not minimized"
    exit 0
}

rc=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${fuzz_input}" -o /dev/null || rc=$?

test "${rc-0}" -ne 127 || {
    echo "not ok" 1 - "fuzz0005 did not execute img2sixel"
    exit 0
}

test "${rc-0}" -ne 134 || {
    echo "not ok" 1 - "fuzz0005 triggered abort"
    exit 0
}

test "${rc-0}" -ne 139 || {
    echo "not ok" 1 - "fuzz0005 triggered SIGSEGV"
    exit 0
}

echo "ok" 1 - "fuzz0005 empty input is handled without crash"


exit 0
