#!/bin/sh
# TAP test ensuring img2sixel rejects incompatible options (8-bit output conflicts with pipe mode).

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

if run_img2sixel -8 -P </dev/null >/dev/null ; then
    fail 1 "unexpected success: 8-bit output conflicts with pipe mode"
    exit 0
fi

pass 1 "invalid option rejected"
exit 0
