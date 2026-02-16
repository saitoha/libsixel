#!/bin/sh
# TAP test confirming progressive JPEG decoding works end-to-end.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

progressive_jpeg="${TOP_SRCDIR}/images/snake-progressive-16x16.jpg"

run_img2sixel "${progressive_jpeg}" >/dev/null || {
    fail 1 "progressive JPEG conversion failed"
    exit 0
}

pass 1 "progressive JPEG converts"
exit 0
