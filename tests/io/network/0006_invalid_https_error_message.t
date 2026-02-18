#!/bin/sh
# TAP test: malformed HTTPS URL reports a formatted network failure status.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBCURL-}" = 1 || test "${HAVE_WINHTTP-}" = 1 || {
    skip_all "libcurl or WinHTTP support is disabled in this build"
}

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

err_file="${ARTIFACT_LOCAL_DIR}/invalid-https.err"

run_img2sixel --env LC_ALL=C -- 'https:///test' >/dev/null 2>"${err_file}" && {
    fail 1 "malformed HTTPS URL unexpectedly succeeded"
    exit 0
}

# OpenBSD's resolver backend can emit a leading blank line before the
# backend-specific message body. Accept that format and only require that
# stderr contains one of the canonical network failure messages.
#
# The concrete failure point can vary by backend and runtime environment:
# - WinHTTP may fail at CrackUrl/Connect/SendRequest/... stages.
# - libcurl may fail at setopt/perform stages depending on URL parsing.
# Keep the check broad enough to accept backend-consistent failures.
awk '/^curl_easy_/ { ++m } /^WinHttp/ { ++m } /runtime error: unable/ { ++m } END { if (!m) exit 1; }' "${err_file}" || {
    fail 1 "missing formatted network failure message"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2
    exit 0
}

pass 1 "malformed HTTPS URL reports formatted network failure"

exit 0
