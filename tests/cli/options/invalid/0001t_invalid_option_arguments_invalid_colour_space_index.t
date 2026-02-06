#!/bin/sh
# TAP test ensuring img2sixel rejects invalid complexion score.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

if run_img2sixel -C0 "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" </dev/null >/dev/null ; then
    fail 1 "unexpected success: invalid complexion score"
    exit 0
fi

pass 1 "invalid option rejected"
exit 0
