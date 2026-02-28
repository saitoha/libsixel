#!/bin/sh
# TAP test verifying ambiguous option prefix is rejected with a diagnostic.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

label="prefix_ambiguous"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"

run_img2sixel -d sie "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >/dev/null 2>"${err_file}" && {
    fail 1 "ambiguous prefix unexpectedly succeeded"
    exit 0
}

grep 'ambiguous prefix "sie"' "${err_file}" >/dev/null 2>&1 || {
    fail 1 "missing diagnostic for ambiguous prefix"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

pass 1 "ambiguous prefix reports diagnostic"
exit 0
