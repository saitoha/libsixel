#!/bin/sh
# TAP test verifying bash completion output from img2sixel.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

msg=$(run_img2sixel --env IMG2SIXEL_COMPLETION_DIR="${TOP_SRCDIR}/converters/shell-completion" \
                    -1 bash) || {
    echo "not ok" 1 - "bash completion output failed"
    exit 0
}

case "${msg}" in
    *'# bash completion for img2sixel'*)
        ;;
    *)
        echo "not ok" 1 - "missing bash completion header"
        exit 0
        ;;
esac

echo "ok" 1 - "bash completion output available"
exit 0
