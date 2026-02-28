#!/bin/sh
# TAP test verifying unknown -Q suboption values are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

label="quantize_suboption_unknown_value"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"

run_img2sixel -Qk:i=xyz "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>"${err_file}" && {
    fail 1 "unknown -Q suboption value unexpectedly succeeded"
    exit 0
}

grep "unknown suboption value" "${err_file}" >/dev/null 2>&1 || {
    fail 1 "missing unknown suboption value diagnostic"
    exit 0
}

pass 1 "unknown -Q suboption value is rejected"
exit 0
