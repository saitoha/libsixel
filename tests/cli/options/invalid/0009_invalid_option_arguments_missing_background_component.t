#!/bin/sh
# TAP test ensuring img2sixel rejects incomplete background components.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

run_img2sixel -B "#ffff" </dev/null >/dev/null  && {
    fail 1 "unexpected success: invalid background component"
    exit 0
}

pass 1 "invalid option rejected"
exit 0
