#!/bin/sh
# TAP test verifying path suggestions report when no nearby entries exist.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

missing_path="${TOP_SRCDIR}/tests/quant/no-such-file.gpl"
err_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-suggestions-no-nearby.err")
out_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-suggestions-no-nearby.out")

run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 --     -m "${missing_path}"     "${TOP_SRCDIR}/tests/data/inputs/snake_64.png"     >"${out_file}" 2>"${err_file}" && {
    fail 1 "missing mapfile unexpectedly succeeded"
    exit 0
}

has_no_nearby=1
grep 'No nearby matches were found in' "${err_file}" >/dev/null 2>&1 &&     has_no_nearby=0

has_fallback=1
grep 'Suggestion lookup unavailable on this build.' "${err_file}"     >/dev/null 2>&1 && has_fallback=0

[ "${has_no_nearby}" -eq 0 ] || [ "${has_fallback}" -eq 0 ] || {
    fail 1 "missing no-nearby-matches diagnostic"
    exit 0
}

[ "${has_no_nearby}" -eq 0 ] || {
    pass 1 "missing path reports unsupported suggestion lookup"
    exit 0
}

pass 1 "missing path reports no-nearby-matches diagnostic"
exit 0
