#!/bin/sh
# TAP test verifying fuzzy suggestions are enabled by default for the CLI.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

err_file="${ARTIFACT_LOCAL_DIR}/fuzzy-suggestions-default-enabled.err"

run_img2sixel -r hamnimg "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" -o/dev/null 2>"${err_file}" && {
    fail 1 "distance-2 typo unexpectedly succeeded"
    exit 0
}

grep 'specified desampling method is not supported.' "${err_file}"     >/dev/null 2>&1 || {
    fail 1 "default CLI setup did not emit fuzzy suggestion"
    exit 0
}

grep 'Did you mean:' "${err_file}" >/dev/null 2>&1 || {
    fail 1 "default CLI setup did not emit fuzzy suggestion"
    exit 0
}

pass 1 "default CLI setup keeps fuzzy suggestions enabled"
exit 0
