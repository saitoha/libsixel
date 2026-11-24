#!/bin/sh
# TAP test verifying unique option prefix is accepted without diagnostics.
# Policy: docs/cli/prefix-matching.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=''
status=0

set +x
msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs:scan=ser \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || status=$?
set -x

test "${status}" -eq 0 || {
    printf '\n%s\n' "not ok 1 - unique prefix was rejected"
    printf '%s\n' '--- stderr ---' >&2
    printf '%s\n' "${msg}" >&2
    exit 0
}

test -z "${msg}" && {
    printf '\n%s\n' "ok 1 - unique prefix is accepted"
    exit 0
}

case "${msg}" in
    *"error"*|*"warning"*|*"failed"*)
        printf '\n%s\n' "not ok 1 - unique prefix emitted diagnostics"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
    *)
        ;;
esac

printf '\n%s\n' "ok 1 - unique prefix is accepted"
exit 0
