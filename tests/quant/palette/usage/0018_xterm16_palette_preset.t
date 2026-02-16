#!/bin/sh
# Re-encode Sixel using xterm16 palette preset.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-xterm16.sixel"

run_img2sixel -bxterm16 "${snake_six}" >"${target_sixel}" || {
    fail 1 "xterm16 preset failed"
    exit 0
}

pass 1 "xterm16 preset re-encodes Sixel"

exit 0
