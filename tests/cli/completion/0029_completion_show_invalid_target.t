#!/bin/sh
# TAP test verifying show-completion rejects unknown targets.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"


ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo '1..1'
set -v

run_img2sixel -1 fish >/dev/null && {
    fail 1 "invalid show target unexpectedly succeeded"
    exit 0
}

pass 1 "invalid show target is rejected"
exit 0
