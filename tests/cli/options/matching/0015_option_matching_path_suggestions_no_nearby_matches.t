#!/bin/sh
# TAP test verifying path suggestions report when no nearby entries exist.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

missing_path="${TOP_SRCDIR}/tests/quant/no-such-file.gpl"
err_output=""

err_output=$({ run_img2sixel --env SIXEL_OPTION_PATH_SUGGESTIONS=1 -- \
    -m "${missing_path}" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >/dev/null; } 2>&1) && {
    fail 1 "missing mapfile unexpectedly succeeded"
    exit 0
}

printf '%s\n' "${err_output}" | grep -F 'No nearby matches were found in' \
    >/dev/null 2>&1 && {
    pass 1 "missing path reports no-nearby-matches diagnostic"
    exit 0
}

printf '%s\n' "${err_output}" | \
    grep -F 'Suggestion lookup unavailable on this build.' \
    >/dev/null 2>&1 || {
    fail 1 "missing no-nearby-matches diagnostic"
    exit 0
}

pass 1 "missing path reports unsupported suggestion lookup"
exit 0
