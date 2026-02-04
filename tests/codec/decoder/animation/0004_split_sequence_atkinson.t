#!/bin/sh
# TAP test: sequence splitting with Atkinson dithering.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${test_name}"
log_file="${artifact_dir}/animation.log"
output_file="${artifact_dir}/split-sequence.sixel"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

image_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

require_file "${image_gif}"

if run_img2sixel -S -datkinson "${image_gif}" >"${output_file}" 2>>"${log_file}"; then
    pass 1 "sequence splitting with Atkinson works"
else
    fail 1 "sequence splitting with Atkinson fails"
fi

exit "${status}"
