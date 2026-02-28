#!/bin/sh
# TAP test verifying missing directories are reported with path suggestions on.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

err_file="${ARTIFACT_LOCAL_DIR}/path-suggestions-missing-dir.err"

run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
              -m "${ARTIFACT_LOCAL_DIR}/not-there/map.gpl" \
              "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
              -o/dev/null 2>"${err_file}" && {
    echo "not ok" 1 "missing directory unexpectedly succeeded"
    exit 0
}

has_missing_directory=1
grep 'Directory "' "${err_file}" >/dev/null 2>&1 || has_missing_directory=0

has_missing_directory_text=1
grep 'does not exist.' "${err_file}" >/dev/null 2>&1 || has_missing_directory_text=0

test "${has_missing_directory}" -eq 1 || {
    echo "not ok" 1 "missing directory diagnostics were not emitted"
    exit 0
}

test "${has_missing_directory_text}" -eq 1 || {
    echo "not ok" 1 "missing directory path reports unsupported suggestion lookup"
    exit 0
}

echo "ok" 1 "missing directory diagnostic is emitted"
exit 0
