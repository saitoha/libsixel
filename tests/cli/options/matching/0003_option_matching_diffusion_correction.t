#!/bin/sh
# TAP test verifying distance-1 typo is corrected or rejected with expected message.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

label="distance1_single"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"
status=0

: >"${err_file}"
: >"${out_file}"

run_img2sixel -d burkez "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" || status=$?

test "${status}" -eq 0 && {
    grep 'corrected --diffusion value "burkez" -> "burkes".' \
        "${err_file}" >/dev/null 2>&1 || {
        fail 1 "missing correction notice"
        printf '%s\n' '--- stderr ---' >&2
        cat "${err_file}" >&2 2>/dev/null || :
        exit 0
    }
    pass 1 "distance-1 typo is corrected"
    exit 0
}

grep 'specified diffusion method is not supported.' "${err_file}" \
    >/dev/null 2>&1 || {
    fail 1 "unexpected rejection without diagnostic"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

pass 1 "distance-1 typo rejected with diagnostic"
exit 0
