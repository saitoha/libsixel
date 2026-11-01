#!/bin/sh
# TAP test verifying sixel2png reports missing required arguments.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -o/dev/null -i 2>&1) && {
    echo "not ok" 1 - "-i without value should fail"
    exit 0
}

case "${msg}" in
    *"missing"*)
        ;;
    *)
        echo "not ok" 1 - "error message did not mention missing input"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *"--input"*)
        ;;
    *)
        echo "not ok" 1 - "error message did not mention missing input"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "missing input argument reported"
exit 0
