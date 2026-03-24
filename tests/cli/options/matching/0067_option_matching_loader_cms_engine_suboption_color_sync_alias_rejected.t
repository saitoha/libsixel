#!/bin/sh
# TAP test verifying loader cms_engine suboption rejects alias value color-sync.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel -Lbuiltin:cms_engine=color-sync! \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "cms_engine=color-sync suboption unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *"\"color-sync\""*"\"cms_engine\""*"valid values"*"none, auto, builtin, lcms2, colorsync"*)
        ;;
    *)
        echo "not ok" 1 - "cms_engine=color-sync suboption rejection message is incomplete"
        exit 0
        ;;
esac

echo "ok" 1 - "cms_engine=color-sync suboption is rejected (strict choices)"
exit 0
