#!/bin/sh
# TAP test: sequence splitting with Atkinson dithering.

set -eux

output_file="${ARTIFACT_LOCAL_DIR}/split-sequence.sixel"


script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

require_file "${image_gif}"

if run_img2sixel -S -datkinson "${image_gif}" >"${output_file}"; then
    pass 1 "sequence splitting with Atkinson works"
else
    fail 1 "sequence splitting with Atkinson fails"
fi

exit "${status}"
