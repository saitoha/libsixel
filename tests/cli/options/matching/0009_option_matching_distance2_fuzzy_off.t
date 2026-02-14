#!/bin/sh
# TAP test verifying fuzzy suggestions can be disabled for invalid choices.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

label="distance2_fuzzy_off"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"

: >"${err_file}"
: >"${out_file}"

run_img2sixel --env SIXEL_OPTION_FUZZY_SUGGESTIONS=0 -- \
    -r hamnimg "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" && {
    fail 1 "distance-2 typo unexpectedly succeeded"
    exit 0
}

grep 'specified desampling method is not supported.' "${err_file}" \
    >/dev/null 2>&1 || {
    fail 1 "invalid choice still reports fuzzy suggestion"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

grep 'Did you mean:' "${err_file}" >/dev/null 2>&1 && {
    fail 1 "invalid choice still reports fuzzy suggestion"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

pass 1 "invalid choice omits fuzzy suggestion when disabled"
exit 0
