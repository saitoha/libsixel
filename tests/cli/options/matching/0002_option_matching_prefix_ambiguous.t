#!/bin/sh
# TAP test verifying ambiguous option prefix is rejected with a diagnostic.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

msg=''
status=0

set +x
msg=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DIAG_MODE=code \
    --env SIXEL_DIAG_MODE_QUIET=1 \
    -d st "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || status=$?
set -x

test "${status}" -eq 2 || {
    echo "not ok" 1 - "ambiguous prefix exit status mismatch"
    exit 0
}

case "${msg}" in
    *'ambiguous prefix "st"'*)
        ;;
    *)
        echo "not ok" 1 - "missing diagnostic for ambiguous prefix"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "ambiguous prefix reports diagnostic"
exit 0
