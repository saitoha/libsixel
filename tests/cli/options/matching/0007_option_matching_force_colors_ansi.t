#!/bin/sh
# TAP test verifying forced color mode injects ANSI sequences in diagnostics.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

label="force_colors_ansi"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"
esc_char=$(printf '\033')

rm -f "${err_file}" "${out_file}"

if run_img2sixel --env SIXEL_STATUS_FORCE_COLORS=1 -- \
        -d sie "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
        >"${out_file}" 2>"${err_file}"; then
    fail 1 "force colors diagnostic unexpectedly succeeded"
    exit 0
fi

if grep -F "${esc_char}[33m" "${err_file}" >/dev/null 2>&1 &&
   grep -F "${esc_char}[1m" "${err_file}" >/dev/null 2>&1 &&
   grep -F "${esc_char}[0m" "${err_file}" >/dev/null 2>&1; then
    pass 1 "force colors injects ANSI markers"
else
    fail 1 "force colors did not inject ANSI markers"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
fi

exit 0
