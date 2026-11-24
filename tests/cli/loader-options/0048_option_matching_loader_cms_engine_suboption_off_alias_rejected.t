#!/bin/sh
# TAP test verifying loader cms_engine suboption rejects alias value off.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg_off=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=off! \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) && {
    printf '\n%s\n' "not ok 1 - cms_engine=off suboption unexpectedly succeeded"
    exit 0
}

case "${msg_off}" in
    *"\"off\""*"\"cms_engine\""*"valid values"*"none, auto, builtin, lcms2, colorsync"*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - cms_engine=off suboption rejection message is incomplete"
        exit 0
        ;;
esac
printf '\n%s\n' "ok 1 - cms_engine=off suboption is rejected (strict choices)"

exit 0
