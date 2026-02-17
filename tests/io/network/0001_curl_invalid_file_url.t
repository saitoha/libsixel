#!/bin/sh
# TAP test: img2sixel rejects invalid file URL without producing output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

feature_defined_in_config "HAVE_LIBCURL" || feature_defined_in_config "HAVE_WINHTTP" || {
    skip_all "libcurl or WinHTTP support is disabled in this build"
}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

set +e
capture_output=$(run_img2sixel 'file:///test')
set -e

test -z "${capture_output}" || {
    fail 1 "invalid file URL produced output"
    exit 0
}

pass 1 "rejects invalid file URL"
exit 0
