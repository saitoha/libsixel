#!/bin/sh
# TAP test ensuring issue #131 PoC GIF is rejected without crashing.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

issue131="${TOP_SRCDIR}/tests/data/security/issue/data/131/2020-01-30-img2sixel.gif"

run_img2sixel -S -Lbuiltin! --high-color "${issue131}" \
    -o"${ARTIFACT_LOCAL_DIR}/issue131-high-color.sixel" || rc=$?

# Expected behavior:
# - The PoC must be rejected (non-zero status).
# - It must not crash (exit 139 indicates SIGSEGV).
test "${rc-0}" -ne 0 || {
    fail 1 "issue #131 PoC unexpectedly accepted"
    exit 0
}

test "${rc-0}" -ne 127 || {
    fail 1 "img2sixel was not executed as expected"
    exit 0
}

test "${rc-0}" -ne 139 || {
    fail 1 "issue #131 PoC triggered SIGSEGV"
    exit 0
}

pass 1 "issue #131 PoC rejected without crashing"

exit 0
