#!/bin/sh
# TAP test ensuring img2sixel rejects invalid find type.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

if run_img2sixel -f invalid_option </dev/null >/dev/null; then
    fail 1 "unexpected success: unknown find type"
    exit 0
fi

pass 1 "invalid option rejected"
exit 0
