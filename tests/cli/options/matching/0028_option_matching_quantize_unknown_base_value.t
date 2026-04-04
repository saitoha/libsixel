#!/bin/sh
# TAP test verifying unknown -Q base tokens include diagnostics and full help.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qzzzmodel \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) && {
    echo "not ok" 1 - "unknown -Q base token unexpectedly succeeded"
    exit 0
}

test "${msg#*unknown option base value*\"zzzmodel\"*valid values*auto*}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing token/candidate details for unknown -Q token"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*valid values: auto, heckbert, kmeans, medoids*}" \
    != "${msg}" || {
    echo "not ok" 1 - "candidate list does not match documented values"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*valid values: auto, heckbert, kmeans, k, medoids*}" \
    = "${msg}" || {
    echo "not ok" 1 - "unexpected explicit shorthand leaked into candidate list"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test "${msg#*:bandit_batch=COUNT*}" != "${msg}" || {
    echo "not ok" 1 - "quantize help text was truncated in invalid -Q diagnostics"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

echo "ok" 1 - "unknown -Q base token reports token and candidates"
exit 0
