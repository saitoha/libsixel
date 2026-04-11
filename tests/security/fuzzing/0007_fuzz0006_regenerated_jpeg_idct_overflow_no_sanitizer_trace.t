#!/bin/sh
# TAP test for fuzz0006: regenerated JPEG avoids sanitizer traces and crashes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

fuzz_input="${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0006/jpeg_idct_overflow_regenerated.jpg"

test -f "${fuzz_input}" || {
    echo "not ok 1 - fuzz0006 regenerated input is missing"
    exit 0
}

rc=0
set +e
err="$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${fuzz_input}" -o /dev/null 2>&1)"
rc=$?
set -e

test "${rc-0}" -ne 127 || {
    echo "not ok 1 - fuzz0006 did not execute img2sixel"
    exit 0
}

test "${rc-0}" -ne 134 || {
    echo "not ok 1 - fuzz0006 triggered abort"
    exit 0
}

test "${rc-0}" -ne 139 || {
    echo "not ok 1 - fuzz0006 triggered SIGSEGV"
    exit 0
}

test "${err#*runtime error:*}" = "${err}" || {
    echo "not ok 1 - fuzz0006 emitted UBSAN runtime error trace"
    exit 0
}

test "${err#*UndefinedBehaviorSanitizer*}" = "${err}" || {
    echo "not ok 1 - fuzz0006 emitted UBSAN summary"
    exit 0
}

test "${err#*AddressSanitizer*}" = "${err}" || {
    echo "not ok 1 - fuzz0006 emitted ASAN trace"
    exit 0
}

echo "ok 1 - fuzz0006 regenerated JPEG is handled without sanitizer trace"


exit 0
