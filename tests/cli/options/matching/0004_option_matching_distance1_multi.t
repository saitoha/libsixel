#!/bin/sh
# TAP test verifying distance-1 multi-match diffusion option is rejected.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

label="distance1_multi"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"

run_img2sixel -r hamning "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >/dev/null 2>"${err_file}" && {
    echo "not ok" 1 "distance-1 multi-match unexpectedly succeeded"
    exit 0
}

grep 'specified desampling method is not supported.' "${err_file}" \
    >/dev/null 2>&1 || {
    echo "not ok" 1 "missing diagnostic for distance-1 multi-match"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

echo "ok" 1 "distance-1 multi-match reports diagnostic"
exit 0
