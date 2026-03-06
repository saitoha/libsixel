#!/bin/sh
# TAP test: img2sixel rejects malformed HTTPS URL without output.

set -eux

test "${HAVE_LIBCURL-}" = 1 || test "${HAVE_WINHTTP-}" = 1 || test "${HAVE_LIBFETCH-}" = 1 || {
    printf "1..0 # SKIP network backend support is disabled in this build\n"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

set +e
capture_output=$(run_img2sixel 'https:///test' 2>/dev/null)
set -e

test -z "${capture_output}" || {
    echo "not ok" 1 - "malformed HTTPS URL produced output"
    exit 0
}

echo "ok" 1 - "rejects malformed HTTPS URL"
exit 0
