#!/bin/sh
# TAP test ensuring img2sixel rejects unknown palette type options.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

run_img2sixel -t invalid_option </dev/null >/dev/null  && {
    fail 1 "unexpected success: unknown palette type options"
    exit 0
}

pass 1 "invalid option rejected"
exit 0
