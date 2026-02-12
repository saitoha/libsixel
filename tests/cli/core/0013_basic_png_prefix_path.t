#!/bin/sh
# TAP test ensuring png: prefix writes to filesystem path.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

out_path="${ARTIFACT_LOCAL_DIR}/out.png"
: >"${out_path}"

run_sixel2png -o "png:${out_path}" <"${images_dir}/map8.six" || {
    fail 1 "prefixed output command failed"
    exit 0
}

test -s "${out_path}" || {
    fail 1 "prefixed output missing"
    exit 0
}

pass 1 "prefixed output trims scheme"
exit 0
