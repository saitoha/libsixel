#!/bin/sh
# TAP test: animation without loop and delay succeeds (builtin loader).

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

run_img2sixel -Lbuiltin! -ldisable -dnone -g "${image_gif}" >/dev/null || {
    fail 1 "static frame rendering fails"
    exit 0
}

pass 1 "static frame rendering succeeds"
exit 0
