#!/bin/sh
# TAP test: invalid HTTPS endpoint reports a formatted network failure status.

set -eux

test "${HAVE_LIBCURL-}" = 1 || test "${HAVE_WINHTTP-}" = 1 || {
    printf "1..0 # SKIP libcurl or WinHTTP support is disabled in this build\n"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

# Use localhost:1 to force a network-layer failure with a syntactically
# valid HTTPS URL. This avoids parser-dependent fallback paths where malformed
# URLs are treated as local filenames.
set +e
capture_output=$(run_img2sixel --env LC_ALL=C -- \
    'https://127.0.0.1:1/test' 2>&1 >/dev/null)
command_status=$?
set -e

test "${command_status}" -ne 0 || {
    fail 1 "invalid HTTPS endpoint unexpectedly succeeded"
    exit 0
}

# The concrete failure point can vary by backend and runtime environment:
# - WinHTTP may fail at CrackUrl/Connect/SendRequest/... stages.
# - libcurl may fail at setopt/perform stages depending on URL parsing.
# Keep the check broad enough to accept backend-consistent failures.
printf '%s\n' "${capture_output}" |
awk '/^curl_easy_/ { ++m } /^WinHttp/ { ++m } /runtime error: unable/ { ++m } END { if (!m) exit 1; }' || {
    fail 1 "missing formatted network failure message"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${capture_output}" >&2
    exit 0
}

pass 1 "invalid HTTPS endpoint reports formatted network failure"

exit 0
