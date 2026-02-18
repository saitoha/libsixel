#!/bin/sh
# TAP test verifying unknown -Q suboption keys are rejected.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

label="quantize_suboption_unknown_key"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"

: >"${out_file}"
: >"${err_file}"

run_img2sixel -Qk:z=p "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" && {
    fail 1 "unknown -Q suboption key unexpectedly succeeded"
    exit 0
}

grep "unknown suboption key" "${err_file}" >/dev/null || {
    fail 1 "missing unknown suboption key diagnostic"
    exit 0
}

pass 1 "unknown -Q suboption key is rejected"
exit 0
