#!/bin/sh
# TAP test verifying unknown -Q suboption keys are rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

label="quantize_suboption_unknown_key"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"

run_img2sixel -Qk:z=p "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >/dev/null 2>"${err_file}" && {
    echo "not ok" 1 "unknown -Q suboption key unexpectedly succeeded"
    exit 0
}

grep "unknown suboption key" "${err_file}" >/dev/null || {
    echo "not ok" 1 "missing unknown suboption key diagnostic"
    exit 0
}

echo "ok" 1 "unknown -Q suboption key is rejected"
exit 0
