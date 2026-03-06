#!/bin/sh
# TAP test verifying prefix suggestions can be disabled for ambiguity errors.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

msg=$(set +xv; run_img2sixel --env SIXEL_OPTION_PREFIX_SUGGESTIONS=0 \
    -d sie "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    -o/dev/null 2>&1) && {
    echo "not ok" 1 - "ambiguous prefix unexpectedly succeeded"
    exit 0
}

case "${msg}" in
    *'ambiguous prefix "sie".'*)
        ;;
    *)
        echo "not ok" 1 - "ambiguity diagnostic still contains candidate list"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *'(matches:'*)
        echo "not ok" 1 - "ambiguity diagnostic still contains candidate list"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "ambiguity diagnostic omits candidate list when disabled"
exit 0
