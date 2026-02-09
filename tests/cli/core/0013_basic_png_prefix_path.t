#!/bin/sh
# TAP test ensuring png: prefix writes to filesystem path.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

rm -f "${ARTIFACT_LOCAL_DIR}/out.png"

run_sixel2png -o "png:${ARTIFACT_LOCAL_DIR}/out.png" <"${images_dir}/map8.six" || {
    fail 1 "prefixed output command failed"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/out.png" || {
    fail 1 "prefixed output missing"
    exit 0
}

pass 1 "prefixed output trims scheme"
exit 0
