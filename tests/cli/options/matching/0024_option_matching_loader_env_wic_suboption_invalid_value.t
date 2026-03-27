#!/bin/sh
# TAP test verifying env-provided WIC suboption values are validated.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n";
    exit 0
}


echo "1..1"
set -v

msg=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_PRIORITY_LIST='wic:ico_minsize=abc!' \
        "${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico" \
        -o/dev/null 2>&1
) && {
    echo "not ok" 1 - "invalid env WIC suboption unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"invalid wic suboption"*)
        ;;
    *)
        echo "not ok" 1 - "missing env WIC suboption diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *"\"abc\""*"\"ico_minsize\""*"positive integer"*)
        ;;
    *)
        echo "not ok" 1 - "missing token/expectation details for invalid env WIC suboption"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "invalid env WIC suboption is rejected"
exit 0
