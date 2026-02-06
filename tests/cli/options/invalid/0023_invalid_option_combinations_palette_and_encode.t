#!/bin/sh
# TAP test palette size and monochrome option cannot be combined.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

if run_img2sixel -p16 -e </dev/null >/dev/null ; then
    fail 1 "unexpected success: palette size and monochrome option conflict"
    exit 0
fi

pass 1 "invalid option rejected"
exit 0
