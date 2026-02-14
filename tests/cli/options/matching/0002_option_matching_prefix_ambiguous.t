#!/bin/sh
# TAP test verifying ambiguous option prefix is rejected with a diagnostic.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

label="prefix_ambiguous"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"

: >"${err_file}"
: >"${out_file}"

run_img2sixel -d sie "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" && {
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
