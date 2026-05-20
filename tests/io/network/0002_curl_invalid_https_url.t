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


echo "1..1"
set -v

set +e
# OpenBSD libcurl resolves "https:///test" as a host lookup and can spend the
# full network timeout before failing. Use a hostless HTTPS form that
# backends reject immediately at URL-parse time instead.
capture_output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" 'https://' 2>/dev/null)
set -e

test -z "${capture_output}" || {
    echo "not ok" 1 - "malformed HTTPS URL produced output"
    exit 0
}

echo "ok" 1 - "rejects malformed HTTPS URL"
exit 0
