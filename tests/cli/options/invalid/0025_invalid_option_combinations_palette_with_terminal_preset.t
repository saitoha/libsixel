#!/bin/sh
# TAP test ensuring palette size conflicts with builtin palette option. 

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

if run_img2sixel -p64 -bxterm256 </dev/null >/dev/null ; then
    fail 1 "unexpected success: palette size conflicts with builtin palette option"
    exit 0
fi

pass 1 "invalid option rejected"
exit 0
