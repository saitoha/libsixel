#!/bin/sh
# TAP test verifying missing directories are reported with path suggestions on.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

err_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-suggestions-missing-dir.err")
out_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-suggestions-missing-dir.out")

run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 -- \
    -m "${ARTIFACT_LOCAL_DIR}/not-there/map.gpl" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" && {
    fail 1 "missing directory unexpectedly succeeded"
    exit 0
}

has_missing_directory=1
grep -F 'Directory "' "${err_file}" >/dev/null 2>&1 &&
    has_missing_directory=0

has_missing_directory_text=1
grep -F 'does not exist.' "${err_file}" >/dev/null 2>&1 &&
    has_missing_directory_text=0

has_fallback=1
grep -F 'Suggestion lookup unavailable on this build.' "${err_file}" \
    >/dev/null 2>&1 && has_fallback=0

[ "${has_missing_directory}" -eq 0 ] || [ "${has_fallback}" -eq 0 || {
    fail 1 "missing directory diagnostics were not emitted"
    exit 0
}

[ "${has_missing_directory}" -ne 0 ] ||
    [ "${has_missing_directory_text}" -eq 0 ] || {
    fail 1 "missing directory diagnostic was not emitted"
    exit 0
}

[ "${has_missing_directory}" -eq 0 ] || {
    pass 1 "missing directory path reports unsupported suggestion lookup"
    exit 0
}

pass 1 "missing directory diagnostic is emitted"
exit 0
