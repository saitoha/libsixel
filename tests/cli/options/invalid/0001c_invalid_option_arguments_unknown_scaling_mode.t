#!/bin/sh
# TAP test ensuring img2sixel rejects unknown scaling mode.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

if run_img2sixel -s invalid_option </dev/null >/dev/null; then
    fail 1 "unexpected success: unknown select type"
    exit 0
fi

pass 1 "invalid option rejected"
exit 0
