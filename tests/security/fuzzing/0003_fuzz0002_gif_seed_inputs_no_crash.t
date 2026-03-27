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

rc=0
run_img2sixel -Lbuiltin! "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_truncated_lzw.gif" -o /dev/null || rc=$?
test "${rc-0}" -ge 1 -a "${rc-0}" -le 3 || {
    echo "not ok" 1 - "fuzz0002 truncated LZW did not return mapped error"
    exit 0
}
test "${rc-0}" -ne 127 || {
    echo "not ok" 1 - "fuzz0002 truncated LZW did not execute img2sixel"
    exit 0
}
test "${rc-0}" -ne 139 || {
    echo "not ok" 1 - "fuzz0002 truncated LZW triggered SIGSEGV"
    exit 0
}

rc=0
run_img2sixel -Lbuiltin! "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_truncated_extension.gif" -o /dev/null || rc=$?
test "${rc-0}" -ge 1 -a "${rc-0}" -le 3 || {
    echo "not ok" 1 - "fuzz0002 truncated extension did not return mapped error"
    exit 0
}
test "${rc-0}" -ne 127 || {
    echo "not ok" 1 - "fuzz0002 truncated extension did not execute img2sixel"
    exit 0
}
test "${rc-0}" -ne 139 || {
    echo "not ok" 1 - "fuzz0002 truncated extension triggered SIGSEGV"
    exit 0
}

rc=0
run_img2sixel -Lbuiltin! "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_bad_lzw_cs0.gif" -o /dev/null || rc=$?
test "${rc-0}" -ge 1 -a "${rc-0}" -le 3 || {
    echo "not ok" 1 - "fuzz0002 bad min code size 0 did not return mapped error"
    exit 0
}
test "${rc-0}" -ne 127 || {
    echo "not ok" 1 - "fuzz0002 bad min code size 0 did not execute img2sixel"
    exit 0
}
test "${rc-0}" -ne 139 || {
    echo "not ok" 1 - "fuzz0002 bad min code size 0 triggered SIGSEGV"
    exit 0
}

rc=0
run_img2sixel -Lbuiltin! "${TOP_SRCDIR}/tests/data/security/fuzzing/data/fuzz0002/gif_seed_bad_lzw_cs9.gif" -o /dev/null || rc=$?
test "${rc-0}" -ge 1 -a "${rc-0}" -le 3 || {
    echo "not ok" 1 - "fuzz0002 bad min code size 9 did not return mapped error"
    exit 0
}
test "${rc-0}" -ne 127 || {
    echo "not ok" 1 - "fuzz0002 bad min code size 9 did not execute img2sixel"
    exit 0
}
test "${rc-0}" -ne 139 || {
    echo "not ok" 1 - "fuzz0002 bad min code size 9 triggered SIGSEGV"
    exit 0
}

echo "ok" 1 - "fuzz0002 GIF seed corpus handled safely"

exit 0
