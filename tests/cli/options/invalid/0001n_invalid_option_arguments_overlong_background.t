#!/bin/sh
# TAP test ensuring img2sixel rejects overlong background specification.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

run_img2sixel -B '#0000000000000' "${images_dir}/map8.png" >/dev/null && {
    fail 1 "unexpected success: unknown background specification"
    exit 0
}

pass 1 "invalid option rejected"
exit 0
