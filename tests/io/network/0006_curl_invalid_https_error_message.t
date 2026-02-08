#!/bin/sh
# TAP test: malformed HTTPS URL reports a formatted loader failure status.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_network_backend_available
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

err_file="${ARTIFACT_LOCAL_DIR}/curl-invalid-https.err"
out_file="${ARTIFACT_LOCAL_DIR}/curl-invalid-https.six"

rm -f "${err_file}" "${out_file}"

if run_img2sixel --env LC_ALL=C -- 'https:///test' \
        >"${out_file}" 2>"${err_file}"; then
    fail 1 "malformed HTTPS URL unexpectedly succeeded"
    exit 0
fi

if [ -n "$(sed -n '1p' "${err_file}")" ] \
        && (grep -F 'curl_easy_perform() failed.' "${err_file}" >/dev/null 2>&1 \
            || grep -F 'runtime error: unable to decode input with available loaders' \
                "${err_file}" >/dev/null 2>&1); then
    pass 1 "malformed HTTPS URL reports formatted network failure"
else
    fail 1 "missing formatted network failure message"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
fi

exit 0
