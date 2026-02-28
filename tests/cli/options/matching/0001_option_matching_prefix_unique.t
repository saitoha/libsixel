#!/bin/sh
# TAP test verifying unique option prefix is accepted without diagnostics.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

label="prefix_unique"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
filtered_err="${ARTIFACT_LOCAL_DIR}/${label}.filtered.err"

echo "1..1"
set -v

run_img2sixel -y ser \
              "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
              >/dev/null 2>"${err_file}" || {
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
    | grep -i 'error\|warning\|failed' \
    >"${filtered_err}" || :

test -s "${filtered_err}" && {
    fail 1 "unique prefix emitted diagnostics"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

pass 1 "unique prefix is accepted"
exit 0
