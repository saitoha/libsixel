#!/bin/sh
# TAP test verifying -m requires an argument and does not shift unexpectedly.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

image_path="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
err_file="${ARTIFACT_LOCAL_DIR}/missing-map.err"
out_file="${ARTIFACT_LOCAL_DIR}/missing-map.out"

: >"${err_file}"
: >"${out_file}"

run_img2sixel -m -w 100 -h 100 "${image_path}" >"${out_file}" 2>"${err_file}" && {
    fail 1 "accepted -m without argument"
    exit 0
}

grep -q 'missing required argument for -m,--mapfile option' "${err_file}" || {
    fail 1 "no diagnostic for missing -m argument"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

pass 1 "reports missing mapfile argument"
exit 0
