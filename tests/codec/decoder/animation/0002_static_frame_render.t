#!/bin/sh
# TAP test: static frame rendering succeeds without updates.

set -eux

output_file="${ARTIFACT_LOCAL_DIR}/static-frame.sixel"


script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

require_file "${image_gif}"

if run_img2sixel -ldisable -dnone -g "${image_gif}" >"${output_file}"; then
    pass 1 "static frame rendering succeeds"
else
    fail 1 "static frame rendering fails"
fi

exit "${status}"
