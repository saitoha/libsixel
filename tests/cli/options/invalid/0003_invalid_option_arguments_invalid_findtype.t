#!/bin/sh
# TAP test ensuring img2sixel rejects invalid findtype names.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

run_img2sixel -f "" </dev/null >/dev/null  && {
    fail 1 "unexpected success: invalid findtype name"
    exit 0
}

pass 1 "invalid option rejected"
exit 0
