#!/bin/sh
# TAP test verifying distance-1 multi-match diffusion option is rejected.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

label="distance1_multi"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"

: >"${err_file}"
: >"${out_file}"

run_img2sixel -r hamning "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" && {
    fail 1 "distance-1 multi-match unexpectedly succeeded"
    exit 0
}

grep 'specified desampling method is not supported.' "${err_file}" \
    >/dev/null 2>&1 || {
    fail 1 "missing diagnostic for distance-1 multi-match"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

pass 1 "distance-1 multi-match reports diagnostic"
exit 0
