#!/bin/sh
# TAP test verifying remote paths bypass local existence checks.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=''
status=0

set +x
msg=$(${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "https://127.0.0.1:1/test.six" \
    -o/dev/null 2>&1) || status=$?
set -x

test "${status}" -ne 0 || {
    printf '\n%s\n' "not ok 1 - remote input unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *'path "https://127.0.0.1:1/test.six" not found.'*)
        printf '\n%s\n' "not ok 1 - remote path was validated as a local filesystem path"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
    *)
        ;;
esac

printf '\n%s\n' "ok 1 - remote path bypassed local filesystem existence checks"
exit 0
