#!/bin/sh
# TAP test verifying sixel2png reports version and exits successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

version_output="${ARTIFACT_LOCAL_DIR}/version.txt"

run_sixel2png -V >"${version_output}" || {
    fail 1 "-V exited with failure"
    exit 0
}

grep -Eq '^sixel2png ' "${version_output}" || {
    fail 1 "version header missing"
    exit 0
}

pass 1 "-V prints version"
exit 0
