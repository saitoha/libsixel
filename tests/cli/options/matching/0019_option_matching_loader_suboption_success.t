#!/bin/sh
# TAP test verifying -L accepts WIC suboptions.

set -euxv

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"

probe_output=$(
    set +xv
    run_img2sixel -Lwic:ico_minsize=40! \
        "${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico" \
        -o/dev/null 2>&1
) || probe_status=$?

case "${probe_output}" in
    *"invalid argument for -L,--loaders option"*)
        echo "not ok" 1 - "-L wic suboptions were rejected by option parser"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${probe_output}" >&2
        exit 0
        ;;
    *)
        ;;
esac

test "${probe_status-}" = "" || {
    echo "not ok" 1 - "-L wic suboptions were rejected by option parser"
    exit 0
}

echo "ok" 1 - "-L accepts wic:ico_minsize suboptions"
exit 0
