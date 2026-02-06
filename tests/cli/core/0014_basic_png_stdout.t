#!/bin/sh
# TAP test verifying png:- writes PNG data to stdout.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

png_stdout="${ARTIFACT_LOCAL_DIR}/png-stdout.png"

run_sixel2png -o "png:-" <"${images_dir}/snake.six" >"${png_stdout}" || {
    fail 1 "png:- command failed"
    exit 0
}

test -s "${png_stdout}" || {
    fail 1 "png:- produced empty output"
    exit 0
}

pass 1 "png:- writes to stdout"
exit 0
