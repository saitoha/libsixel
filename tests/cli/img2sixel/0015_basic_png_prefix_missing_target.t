#!/bin/sh
# TAP test ensuring an empty png: prefix is rejected.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -o "png:" <"${TOP_SRCDIR}/images/map8.six" \
        2>&1) && {
    printf '\n%s\n' "not ok 1 - accepts empty png: prefix"
    exit 0
}

case "${msg}" in
    *'missing target after the "png:" prefix'*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - missing png prefix diagnostic"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - rejects empty png prefix"
exit 0
