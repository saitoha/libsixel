#!/bin/sh
# TAP test verifying sixel2png reports usage information.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

help_output="${ARTIFACT_LOCAL_DIR}/help.txt"
if run_sixel2png -H 1>"${help_output}"; then
    if grep -Eq '^Usage: sixel2png' "${help_output}"; then
        pass 1 "-H prints usage"
    else
        fail 1 "help usage header missing"
    fi
else
    fail 1 "-H exited with failure"
fi

exit "${status}"
