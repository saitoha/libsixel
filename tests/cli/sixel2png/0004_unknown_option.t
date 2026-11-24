#!/bin/sh
# TAP test verifying sixel2png rejects unknown options gracefully.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" --unknown -o/dev/null 2>&1) && {
    printf '\n%s\n' "not ok 1 - unknown option should fail"
    exit 0
}

case "${msg}" in
    *"unrecognized option"*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - error message did not mention unknown option"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - unknown option reported"
exit 0
