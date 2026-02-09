#!/bin/sh
# TAP test converting snake.six with explicit file arguments.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

run_sixel2png -i "${images_dir}/map8.six" -o "${ARTIFACT_LOCAL_DIR}/output.png" || {
    fail 1 "snake file conversion failed"
    exit 0
}

pass 1 "converts snake with file arguments"
exit 0
