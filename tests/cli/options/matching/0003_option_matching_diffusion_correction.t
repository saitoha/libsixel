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

rm -f "${err_file}" "${out_file}"

if run_img2sixel -d burkez "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" >"${out_file}" 2>"${err_file}"; then
    if grep -F 'corrected --diffusion value "burkez" -> "burkes".' \
            "${err_file}" >/dev/null 2>&1; then
        pass 1 "distance-1 typo is corrected"
    else
        fail 1 "missing correction notice"
        printf '%s\n' '--- stderr ---' >&2
        cat "${err_file}" >&2 2>/dev/null || :
    fi
else
    if grep -F 'specified diffusion method is not supported.' \
            "${err_file}" >/dev/null 2>&1; then
        pass 1 "distance-1 typo rejected with diagnostic"
    else
        fail 1 "unexpected rejection without diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        cat "${err_file}" >&2 2>/dev/null || :
    fi
fi

exit 0
