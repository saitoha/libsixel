#!/bin/sh
# TAP test for fuzz0002: GIF seed corpus returns mapped errors without crash.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

run_seed_case() {
    seed_file="$1"
    label="$2"

    rc=0
    run_img2sixel -Lbuiltin! "${seed_file}" -o /dev/null || rc=$?

    test "${rc-0}" -ge 1 -a "${rc-0}" -le 3 || {
        echo "# ${label}: did not return mapped error"
        return 1
    }

    test "${rc-0}" -ne 127 || {
        echo "# ${label}: did not execute img2sixel"
        return 1
    }

    test "${rc-0}" -ne 139 || {
        echo "# ${label}: triggered SIGSEGV"
        return 1
    }

    return 0
}

failed=0
run_seed_case \
    "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_truncated_lzw.gif" \
    "fuzz0002 truncated LZW" || failed=1
run_seed_case \
    "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_truncated_extension.gif" \
    "fuzz0002 truncated extension" || failed=1
run_seed_case \
    "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_bad_lzw_cs0.gif" \
    "fuzz0002 bad min code size 0" || failed=1
run_seed_case \
    "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_bad_lzw_cs9.gif" \
    "fuzz0002 bad min code size 9" || failed=1

test "${failed}" -eq 0 || {
    echo "not ok" 1 - "fuzz0002 GIF seed corpus handled safely"
    exit 0
}

echo "ok" 1 - "fuzz0002 GIF seed corpus handled safely"

exit 0
