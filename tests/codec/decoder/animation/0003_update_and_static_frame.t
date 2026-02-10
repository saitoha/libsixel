#!/bin/sh
# TAP test: combined update and static frame handling.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

run_img2sixel -ldisable -dnone -u -g "${image_gif}" >/dev/null || {
    fail 1 "combined update and static frame fails"
    exit 0
}

pass 1 "combined update and static frame works"
exit 0
