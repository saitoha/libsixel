#!/bin/sh
# TAP test: img2sixel rejects invalid file URL without producing output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_network_backend_available
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

set +e
capture_output=$(run_img2sixel 'file:///test' 2>/dev/null)
set -e

[ -z "${capture_output}" ] || {
    fail 1 "invalid file URL produced output"
    exit 0
}

pass 1 "rejects invalid file URL"
exit 0
