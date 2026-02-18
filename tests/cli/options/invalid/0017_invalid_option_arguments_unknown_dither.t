#!/bin/sh
# TAP test ensuring img2sixel rejects unknown dither option.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

run_img2sixel -d "" </dev/null >/dev/null  && {
    fail 1 "unexpected success: unknown dither option"
    exit 0
}

pass 1 "invalid option rejected"
exit 0
