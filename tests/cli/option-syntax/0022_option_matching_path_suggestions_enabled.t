#!/bin/sh
# TAP test verifying path suggestions are emitted for missing files.
# Policy: docs/cli/correction-suggestions.md

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
    --env SIXEL_OPTION_PATH_SUGGESTIONS=1 \
    --env SIXEL_DIAG_MODE=code \
    --env SIXEL_DIAG_MODE_QUIET=1 \
    -m "${TOP_SRCDIR}/tests/data/inputs/small.ppn" \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" -o/dev/null 2>&1) || status=$?
set -x

test "${status}" -eq 2 || {
    printf '\n%s\n' "not ok 1 - missing mapfile exit status mismatch"
    exit 0
}

case "${msg}" in
    *'path "'*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - missing path suggestion diagnostics"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *'Suggestions:'*)
        ;;
    *)
        printf '\n%s\n' "not ok 1 - missing mapfile reports unsupported suggestion lookup"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

printf '\n%s\n' "ok 1 - missing mapfile prints ranked path suggestions"
exit 0
