#!/bin/sh
# TAP test verifying fuzzy suggestions can be disabled for invalid choices.

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
msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_OPTION_FUZZY_SUGGESTIONS=0 \
    -r hamnimg "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || status=$?
set -x

test "${status}" -eq 2 || {
    echo "not ok" 1 - "distance-2 typo exit status mismatch"
    exit 0
}

case "${msg}" in
    *'specified desampling method is not supported.'*)
        ;;
    *)
        echo "not ok" 1 - "invalid choice still reports fuzzy suggestion"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *'Did you mean:'*)
        echo "not ok" 1 - "invalid choice still reports fuzzy suggestion"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "invalid choice omits fuzzy suggestion when disabled"
exit 0
