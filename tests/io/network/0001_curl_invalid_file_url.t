#!/bin/sh
# TAP test: img2sixel rejects invalid file URL without producing output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBCURL-}" = 1 || test "${HAVE_WINHTTP-}" = 1 || {
    skip_all "libcurl or WinHTTP support is disabled in this build"
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

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
