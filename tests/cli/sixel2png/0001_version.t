#!/bin/sh
# TAP test verifying sixel2png reports version and exits successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

version_output="${ARTIFACT_LOCAL_DIR}/version.txt"
if run_sixel2png -V >"${version_output}"; then
    if grep -Eq '^sixel2png ' "${version_output}"; then
        pass 1 "-V prints version"
    else
        fail 1 "version header missing"
    fi
else
    fail 1 "-V exited with failure"
fi

exit "${status}"
