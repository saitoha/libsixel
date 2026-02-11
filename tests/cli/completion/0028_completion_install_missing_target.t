#!/bin/sh
# TAP test verifying install-completion rejects missing targets.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo '1..1'
set -v

if run_img2sixel -2 >/dev/null 2>&1; then
    fail 1 "missing install target unexpectedly succeeded"
else
    pass 1 "missing install target is rejected"
fi

exit "${status}"
