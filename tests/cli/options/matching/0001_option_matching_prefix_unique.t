#!/bin/sh
# TAP test verifying unique option prefix is accepted without diagnostics.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

label="prefix_unique"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"
filtered_err="${ARTIFACT_LOCAL_DIR}/${label}.filtered.err"

: >"${err_file}"
: >"${out_file}"
: >"${filtered_err}"

echo "1..1"
set -v

run_img2sixel -y ser "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" || {
    fail 1 "unique prefix was rejected"
    exit 0
}

test ! -s "${err_file}" && {
    pass 1 "unique prefix is accepted"
    exit 0
}

sed '1d' "${err_file}" \
    | grep -v '^+' \
    | grep -v 'img2sixel' \
    | grep -Ei 'error|warning|failed' \
    >"${filtered_err}" || :

test ! -s "${filtered_err}" && {
    pass 1 "unique prefix is accepted"
    exit 0
}

cli_core_fail 1 "unique prefix emitted diagnostics"
printf '%s\n' '--- stderr ---' >&2
cat "${err_file}" >&2 2>/dev/null || :
exit 0
