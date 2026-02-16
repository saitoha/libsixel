#!/bin/sh
# TAP test: PAL1 input expands via fallback path when tables are disabled.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_ascii_pbm="${TOP_SRCDIR}/images/snake-ascii.pbm"

SIXEL_PALETTE_DISABLE_TABLES=1
export SIXEL_PALETTE_DISABLE_TABLES
run_img2sixel "${snake_ascii_pbm}" -o "${ARTIFACT_LOCAL_DIR}/snake-fallback.sixel" || {
    printf 'not ok 1 - fallback expansion failed\n'
    exit 0
}

printf 'ok 1 - PAL1 input expands via fallback path\n'
exit 0
