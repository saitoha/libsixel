#!/bin/sh
# TAP test: fallback path produces output data when tables are disabled.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"

tmp_dir="${ARTIFACT_LOCAL_DIR}"


script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

snake_ascii_pbm="${images_dir}/snake-ascii.pbm"
require_file "${snake_ascii_pbm}"

SIXEL_PALETTE_DISABLE_TABLES=1 \
    run_img2sixel "${snake_ascii_pbm}" \
    -o "${output_dir}/snake-fallback.sixel" \
 || true

if [ -s "${output_dir}/snake-fallback.sixel" ]; then
    printf 'ok 1 - fallback output produced data\n'
else
    printf 'not ok 1 - fallback output empty\n'
    exit 1
fi
