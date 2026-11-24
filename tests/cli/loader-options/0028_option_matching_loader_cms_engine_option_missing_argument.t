#!/bin/sh
# TAP test verifying short -# requires an argument.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg_short=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -# 2>&1) && {
    printf '\n%s\n' "not ok 1 - short -# missing argument unexpectedly succeeded"
    exit 0
}

case "${msg_short}" in
    *"requires an argument"*"#"*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - short -# missing argument diagnostic is missing"
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - short -# requires an argument"
exit 0
