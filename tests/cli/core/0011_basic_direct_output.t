#!/bin/sh
# TAP test producing direct RGBA output with sixel2png.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

direct_png="${ARTIFACT_LOCAL_DIR}/output.png"
run_sixel2png -D <"${images_dir}/snake.six" >"${direct_png}" || {
    fail 1 "direct RGBA conversion failed"
    exit 0
}

pass 1 "produces direct RGBA output"
exit 0
