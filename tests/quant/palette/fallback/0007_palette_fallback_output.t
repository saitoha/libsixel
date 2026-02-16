#!/bin/sh
# TAP test: fallback path produces output data when tables are disabled.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_ascii_pbm="${TOP_SRCDIR}/images/snake-ascii.pbm"

run_img2sixel --env SIXEL_PALETTE_DISABLE_TABLES=1 "${snake_ascii_pbm}" -o "${ARTIFACT_LOCAL_DIR}/snake-fallback.sixel" || {
    printf 'not ok 1 - fallback output empty\n'
    exit 0
}

printf 'ok 1 - fallback output produced data\n'
exit 0
