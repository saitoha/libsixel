#!/bin/sh
# TAP test verifying missing directories are reported with path suggestions on.

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
    -m "${TOP_SRCDIR}/tests/data/inputs/__missing_dir__/map.gpl" \
    "${TOP_SRCDIR}/tests/data/inputs/small.ppm" \
    -o/dev/null 2>&1) || status=$?
set -x

test "${status}" -eq 2 || {
    echo "not ok" 1 - "missing directory exit status mismatch"
    exit 0
}

case "${msg}" in
    *'Directory "'*)
        ;;
    *)
        echo "not ok" 1 - "missing directory diagnostics were not emitted"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

case "${msg}" in
    *'does not exist.'*)
        ;;
    *)
        echo "not ok" 1 - "missing directory path reports unsupported suggestion lookup"
        printf '%s\n' '--- stderr ---' >&2
        printf '%s\n' "${msg}" >&2
        exit 0
        ;;
esac

echo "ok" 1 - "missing directory diagnostic is emitted"
exit 0
