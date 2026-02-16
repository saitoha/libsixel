#!/bin/sh
# TAP test: img2sixel rejects malformed HTTPS URL without output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

feature_defined_in_config "HAVE_LIBCURL" || feature_defined_in_config "HAVE_WINHTTP" || {
    skip_all "libcurl or WinHTTP support is disabled in this build"
    return 0
}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

set +e
capture_output=$(run_img2sixel 'https:///test' 2>/dev/null)
set -e

test -z "${capture_output}" || {
    fail 1 "malformed HTTPS URL produced output"
    exit 0
}

pass 1 "rejects malformed HTTPS URL"
exit 0
