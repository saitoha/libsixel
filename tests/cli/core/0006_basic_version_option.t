#!/bin/sh
# TAP test verifying sixel2png prints version information.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

run_sixel2png -V >"${ARTIFACT_LOCAL_DIR}/version.txt" || {
    fail 1 "version option failed"
}

pass 1 "prints version"
exit 0
