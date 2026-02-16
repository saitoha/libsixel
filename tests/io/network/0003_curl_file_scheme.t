#!/bin/sh
# TAP test: img2sixel fetches local files via the file scheme.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

feature_defined_in_config "HAVE_LIBCURL" || {
    skip_all "libcurl support is disabled in this build"
    return 0
}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

local_file="file://${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
run_img2sixel "${local_file}" >"${ARTIFACT_LOCAL_DIR}/local-file.sixel" || {
    fail 1 "local file fetch via file scheme failed"
    exit 0
}

pass 1 "fetches local file via file scheme"
exit 0
