#!/bin/sh
# TAP test converting map8.six from stdin with sixel2png.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

run_sixel2png - <"${images_dir}/map8.six" >"${ARTIFACT_LOCAL_DIR}/map8-stdin.png" || {
    fail 1 "map8 stdin conversion failed"
    exit 0
}

pass 1 "converts map8 from stdin"
exit 0
