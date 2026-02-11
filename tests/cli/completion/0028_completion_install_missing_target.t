#!/bin/sh
# TAP test verifying install-completion rejects missing targets.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"


ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo '1..1'
set -v

run_img2sixel -2 >/dev/null && {
    fail 1 "missing install target unexpectedly succeeded"
    exit 0
}

pass 1 "missing install target is rejected"
exit 0
