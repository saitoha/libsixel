#!/bin/sh
# TAP test ensuring sixel2png rejects missing input paths.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

run_sixel2png -i "${ARTIFACT_LOCAL_DIR}/unknown.six" \
    >"${ARTIFACT_LOCAL_DIR}/output.txt" \
    2>"${ARTIFACT_LOCAL_DIR}/err.txt" && {
    fail 1 "accepts missing input path"
    exit 0
}

pass 1 "rejects missing input path"
exit 0
