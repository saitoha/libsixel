#!/bin/sh
# TAP test checking oversized DCS geometry is tolerated.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

oversized="${TOP_SRCDIR}/tests/data/inputs/snake_64-oversized.six"

run_img2sixel "${oversized}" >/dev/null || {
    fail 1 "oversized DCS geometry rejected"
    exit 0
}

pass 1 "oversized DCS geometry tolerated"
exit 0
