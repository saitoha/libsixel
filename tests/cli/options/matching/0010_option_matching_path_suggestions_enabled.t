#!/bin/sh
# TAP test verifying path suggestions are emitted for missing files.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

err_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-suggestions-enabled.err")
out_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-suggestions-enabled.out")

run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 --     -m "${TOP_SRCDIR}/tests/data/inputs/snake_64.pgn"     "${TOP_SRCDIR}/tests/data/inputs/snake_64.png"     >"${out_file}" 2>"${err_file}" && {
    fail 1 "missing mapfile unexpectedly succeeded"
    exit 0
}

grep 'path "' "${err_file}" >/dev/null 2>&1 || {
    fail 1 "missing path suggestion diagnostics"
    exit 0
}

has_suggestions=1
grep 'Suggestions:' "${err_file}" >/dev/null 2>&1 && has_suggestions=0

has_fallback=1
grep 'Suggestion lookup unavailable on this build.' "${err_file}"     >/dev/null 2>&1 && has_fallback=0

[ "${has_suggestions}" -eq 0 ] || [ "${has_fallback}" -eq 0 ] || {
    fail 1 "missing path suggestion diagnostics"
    exit 0
}

[ "${has_suggestions}" -eq 0 ] || {
    pass 1 "missing mapfile reports unsupported suggestion lookup"
    exit 0
}

pass 1 "missing mapfile prints ranked path suggestions"
exit 0
