#!/bin/sh
# TAP test: animation with disable loop, macro mode, ignore delay flags (builtin loader).

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

run_img2sixel -Lbuiltin! -ldisable -dnone -u -g "${image_gif}" >/dev/null || {
    fail 1 "combined update and static frame fails"
    exit 0
}

pass 1 "combined update and static frame works"
exit 0
