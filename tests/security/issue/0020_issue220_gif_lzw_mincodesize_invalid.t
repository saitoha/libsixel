#!/bin/sh
# TAP test: invalid GIF LZW minimum code sizes are rejected safely.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..3\n'
set -v

check_invalid_lzw() {
    test_no="$1"
    input_file="$2"
    label="$3"

    rc=0
    run_img2sixel -Lbuiltin! "${input_file}" -o /dev/null || rc=$?

    test "${rc-0}" -ge 1 -a "${rc-0}" -le 3 || {
        echo "not ok" "${test_no}" - "${label} did not return mapped error status"
        return
    }

    test "${rc-0}" -ne 127 || {
        echo "not ok" "${test_no}" - "${label} did not execute img2sixel"
        return
    }

    test "${rc-0}" -ne 139 || {
        echo "not ok" "${test_no}" - "${label} triggered SIGSEGV"
        return
    }

    echo "ok" "${test_no}" - "${label} rejected safely"
}

check_invalid_lzw 1 \
    "${TOP_SRCDIR}/tests/data/security/issue/data/220/poc8_gif_lzw_mincodesize0.gif" \
    "min code size 0 GIF"
check_invalid_lzw 2 \
    "${TOP_SRCDIR}/tests/data/security/issue/data/220/poc9_gif_lzw_mincodesize1.gif" \
    "min code size 1 GIF"
check_invalid_lzw 3 \
    "${TOP_SRCDIR}/tests/data/security/issue/data/220/poc10_gif_lzw_mincodesize9.gif" \
    "min code size 9 GIF"

exit 0
