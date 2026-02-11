#!/bin/sh
# TAP test verifying remote paths bypass local existence checks.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

err_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-remote-bypass.err")
out_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "path-remote-bypass.out")

run_sixel2png -i "https://example.invalid/test.six" \
    -o "${out_file}" >/dev/null 2>"${err_file}" && {
    fail 1 "remote input unexpectedly succeeded"
    exit 0
}

grep -F 'path "https://example.invalid/test.six" not found.' \
    "${err_file}" >/dev/null 2>&1 && {
    fail 1 "remote path was validated as a local filesystem path"
    exit 0
}

pass 1 "remote path bypassed local filesystem existence checks"
exit 0
