#!/bin/sh
# TAP test: img2sixel fetches local files via the file scheme.

set -eux

script_dir=${test_dir}
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

if ! feature_defined_in_config "HAVE_LIBCURL"; then
    skip_all "libcurl is required for file scheme access"
fi

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

local_file="file://$(CDPATH=; cd "${top_srcdir}" && pwd)/tests/data/inputs/snake_64.jpg"
if run_img2sixel "${local_file}" \
        >"${ARTIFACT_LOCAL_DIR}/local-file.sixel"; then
    printf 'ok 1 - fetches local file via file scheme\n'
else
    printf 'not ok 1 - local file fetch via file scheme failed\n'
    exit 1
fi
