#!/bin/sh
# TAP test: truncated GIF LZW payload is rejected without crashing.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

issue220="${TOP_SRCDIR}/tests/data/security/issue/data/220/poc6_gif_truncated_lzw.gif"

run_img2sixel -Lbuiltin! "${issue220}" -o /dev/null || rc=$?

test "${rc-0}" -ne 0 || {
    echo "not ok" 1 - "truncated LZW GIF unexpectedly accepted"
    exit 0
}

test "${rc-0}" -ne 127 || {
    echo "not ok" 1 - "img2sixel was not executed as expected"
    exit 0
}

test "${rc-0}" -ne 139 || {
    echo "not ok" 1 - "truncated LZW GIF triggered SIGSEGV"
    exit 0
}

echo "ok" 1 - "truncated LZW GIF rejected without crash"
exit 0
