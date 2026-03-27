#!/bin/sh
# TAP test for fuzz0002: GIF seed corpus returns mapped errors without crash.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..4\n'
set -v

run_seed_case() {
    test_no="$1"
    seed_file="$2"
    label="$3"

    rc=0
    run_img2sixel -Lbuiltin! "${seed_file}" -o /dev/null || rc=$?

    test "${rc-0}" -ge 1 -a "${rc-0}" -le 3 || {
        echo "not ok" "${test_no}" - "${label} did not return mapped error"
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

    echo "ok" "${test_no}" - "${label} handled safely"
}

run_seed_case 1 \
    "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_truncated_lzw.gif" \
    "fuzz0002 truncated LZW"
run_seed_case 2 \
    "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_truncated_extension.gif" \
    "fuzz0002 truncated extension"
run_seed_case 3 \
    "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_bad_lzw_cs0.gif" \
    "fuzz0002 bad min code size 0"
run_seed_case 4 \
    "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_bad_lzw_cs9.gif" \
    "fuzz0002 bad min code size 9"

exit 0
