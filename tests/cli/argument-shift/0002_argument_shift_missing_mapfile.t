#!/bin/sh
# TAP test verifying -m requires an argument and does not shift unexpectedly.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

image_path="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"

msg=$(set +xv; run_img2sixel -m -w 100 -h 100 "${image_path}" -o/dev/null 2>&1) && {
    echo "not ok" 1 "accepted -m without argument"
    exit 0
}

case "${msg}" in
    *'missing required argument for -m,--mapfile option'*)
        ;;
    *)
        echo "not ok" 1 "no diagnostic for missing -m argument"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 "reports missing mapfile argument"
exit 0
