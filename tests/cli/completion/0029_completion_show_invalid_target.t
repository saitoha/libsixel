#!/bin/sh
# TAP test verifying show-completion rejects unknown targets.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo '1..1'
set -v

if run_img2sixel -1 fish >/dev/null 2>&1; then
    fail 1 "invalid show target unexpectedly succeeded"
else
    pass 1 "invalid show target is rejected"
fi

exit "${status}"
