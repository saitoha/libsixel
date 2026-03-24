#!/bin/sh
# TAP test verifying -#/--cms-engine requires an argument.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..2"
set -v

msg_short=$(set +xv; run_img2sixel -# 2>&1) && {
    echo "not ok" 1 - "short -# missing argument unexpectedly succeeded"
    echo "not ok" 2 - "long --cms-engine missing argument unexpectedly succeeded"
    exit 0
}

case "${msg_short}" in
    *"requires an argument"*"#"*)
        ;;
    *)
        echo "not ok" 1 - "short -# missing argument diagnostic is missing"
        echo "not ok" 2 - "long --cms-engine missing argument unexpectedly succeeded"
        exit 0
        ;;
esac

echo "ok" 1 - "short -# requires an argument"

msg_long=$(set +xv; run_img2sixel --cms-engine 2>&1) && {
    echo "not ok" 2 - "long --cms-engine missing argument unexpectedly succeeded"
    exit 0
}

case "${msg_long}" in
    *"--cms-engine"*)
        ;;
    *)
        echo "not ok" 2 - "long --cms-engine option name is missing in diagnostic"
        exit 0
        ;;
esac

case "${msg_long}" in
    *"requires an argument"*)
        ;;
    *)
        echo "not ok" 2 - "long --cms-engine missing argument hint is missing"
        exit 0
        ;;
esac

echo "ok" 2 - "long --cms-engine requires an argument"
exit 0
