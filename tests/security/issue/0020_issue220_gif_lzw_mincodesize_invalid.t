#!/bin/sh
# TAP test: invalid GIF LZW minimum code size 0 is rejected safely.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

input_file="${TOP_SRCDIR}/tests/data/security/issue/data/220/poc8_gif_lzw_mincodesize0.gif"

rc=0
run_img2sixel -Lbuiltin! "${input_file}" -o /dev/null || rc=$?

test "${rc-0}" -ge 1 -a "${rc-0}" -le 3 || {
    echo "not ok" 1 - "min code size 0 GIF did not return mapped error status"
    exit 0
}

test "${rc-0}" -ne 127 || {
    echo "not ok" 1 - "min code size 0 GIF did not execute img2sixel"
    exit 0
}

test "${rc-0}" -ne 139 || {
    echo "not ok" 1 - "min code size 0 GIF triggered SIGSEGV"
    exit 0
}

echo "ok" 1 - "min code size 0 GIF rejected safely"

exit 0
