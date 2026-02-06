#!/bin/sh
# TAP test verifying unique option prefix is accepted without diagnostics.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

label="prefix_unique"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"
filtered_err="${ARTIFACT_LOCAL_DIR}/${label}.filtered.err"

cleanup_files() {
    rm -f "$@" || :
}

cleanup_files "${err_file}" "${out_file}" "${filtered_err}"

echo "1..1"
set -v

if run_img2sixel -y ser "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" >"${out_file}" 2>"${err_file}"; then
    :
else
    fail 1 "unique prefix was rejected"
    exit 0
fi

if [ -s "${err_file}" ]; then
    if sed '1d' "${err_file}" \
            | grep -v '^+' \
            | grep -v 'img2sixel' \
            | grep -Ei 'error|warning|failed' \
            >"${filtered_err}"; then
        if [ -s "${filtered_err}" ]; then
            cli_core_fail 1 "unique prefix emitted diagnostics"
            printf '%s\n' '--- stderr ---' >&2
            cat "${err_file}" >&2 2>/dev/null || :
            cleanup_files "${filtered_err}"
            exit 0
        fi
    fi
    cleanup_files "${filtered_err}"
fi

pass 1 "unique prefix is accepted"
exit 0
