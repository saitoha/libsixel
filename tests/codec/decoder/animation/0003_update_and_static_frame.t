#!/bin/sh
# TAP test: combined update and static frame handling.

set -eux

output_file="${ARTIFACT_LOCAL_DIR}/update-and-static.sixel"


script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

require_file "${image_gif}"

if run_img2sixel -ldisable -dnone -u -g "${image_gif}" >"${output_file}"; then
    pass 1 "combined update and static frame works"
else
    fail 1 "combined update and static frame fails"
fi

exit "${status}"
