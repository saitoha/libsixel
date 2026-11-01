#!/bin/sh
# TAP test: truncated GIF extension block is rejected without crashing.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v

issue220="${TOP_SRCDIR}/tests/data/security/issue/data/220/poc7_gif_truncated_extension.gif"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${issue220}" -o /dev/null || rc=$?

test "${rc-0}" -ne 0 || {
    echo "not ok" 1 - "truncated extension GIF unexpectedly accepted"
    exit 0
}

test "${rc-0}" -ne 127 || {
    echo "not ok" 1 - "img2sixel was not executed as expected"
    exit 0
}

test "${rc-0}" -ne 139 || {
    echo "not ok" 1 - "truncated extension GIF triggered SIGSEGV"
    exit 0
}

echo "ok" 1 - "truncated extension GIF rejected without crash"
exit 0
