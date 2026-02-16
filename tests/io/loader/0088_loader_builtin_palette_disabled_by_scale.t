#!/bin/sh
# TAP test confirming scale options disable builtin palette fast path.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/palette.png"

run_img2sixel -L builtin! -w 32 "${input_png}" >/dev/null || {
    fail 1 "scaled indexed PNG decode failed with builtin loader"
    exit 0
}

pass 1 "scale options disable builtin palette fast path"
exit 0
