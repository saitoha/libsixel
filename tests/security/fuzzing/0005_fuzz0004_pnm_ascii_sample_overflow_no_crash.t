#!/bin/sh
# TAP test for fuzz0004: ASCII PNM sample overflow is rejected without crash.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}


echo "1..1"
set -v

fuzz_input="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0004/pnm_ascii_sample_overflow.p3"

rc=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${fuzz_input}" -o /dev/null || rc=$?

test "${rc-0}" -ge 1 -a \
    "${rc-0}" -le "${SIXEL_TEST_MAX_MAPPED_ERROR_STATUS-3}" || {
    echo "not ok 1 - fuzz0004 did not return mapped error status"
    exit 0
}

test "${rc-0}" -ne 127 || {
    echo "not ok 1 - fuzz0004 did not execute img2sixel"
    exit 0
}

test "${rc-0}" -ne 134 || {
    echo "not ok 1 - fuzz0004 triggered abort"
    exit 0
}

test "${rc-0}" -ne 139 || {
    echo "not ok 1 - fuzz0004 triggered SIGSEGV"
    exit 0
}

echo "ok 1 - fuzz0004 ASCII PNM sample overflow handled safely"


exit 0
