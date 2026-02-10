#!/bin/sh
# TAP test: animation disabled with update mode flags.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

run_img2sixel -ldisable -dnone -u -lauto "${image_gif}" -o/dev/null || {
    fail 1 "animation disable with update failed"
    exit 0
}

pass 1 "animation disabled with update mode"
exit 0
